#ifdef ANXIETY_BACKEND_DX11

#include "DX11Device.h"
#include "DX11CommandBuffer.h"
#include "DX11Swapchain.h"
#include "DX11Shader.h"
#include "DX11Pipeline.h"
#include "DX11DescriptorSet.h"
#include "DX11Helpers.h"
#include "Logger.h"

#include <d3dcompiler.h>
#include <cassert>
#include <algorithm>
#include <cstring>

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    static inline uint32_t to_texture_slot(rhi::TextureHandle h) noexcept { return static_cast<uint32_t>(h.id) - 1u; }
    static inline uint32_t to_buffer_slot(rhi::BufferHandle h)   noexcept { return static_cast<uint32_t>(h.id) - 1u; }

    // Construcción / destrucción -----------------------------------------------------------------
    DX11Device::DX11Device(bool enable_debug_layer) {
        UINT flags = 0;
        if (enable_debug_layer) flags |= D3D11_CREATE_DEVICE_DEBUG;

        // Niveles de característica a probar en orden — primero 11.1, recurre a 11.0
        const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, };
        D3D_FEATURE_LEVEL selected_level{};

        ComPtr<ID3D11Device>        device;
        ComPtr<ID3D11DeviceContext> ctx;

        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels, static_cast<UINT>(std::size(feature_levels)), D3D11_SDK_VERSION, &device, &selected_level, &ctx);

        if (FAILED(hr)) {
            LOG_WARNING(k_category, "La creación del dispositivo D3D11 de hardware falló, probando con WARP.");
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, feature_levels, static_cast<UINT>(std::size(feature_levels)), D3D11_SDK_VERSION, &device, &selected_level, &ctx);
        }

        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "D3D11CreateDevice falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        m_device = device;
        m_imm_ctx = ctx;

        // Deriva IDXGIFactory2 a partir del adaptador elegido -------------------------------------------
        ComPtr<IDXGIDevice>  dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        m_device.As(&dxgiDevice);
        dxgiDevice->GetAdapter(&adapter);
        hr = adapter->GetParent(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "GetParent(IDXGIFactory2) falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Registra en el log el nombre del adaptador
        DXGI_ADAPTER_DESC adapterDesc{};
        adapter->GetDesc(&adapterDesc);
        char name[128]{};
        WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name, static_cast<int>(sizeof(name)), nullptr, nullptr);
        LOGF_INFO(k_category, "Dispositivo DirectX 11 listo (FL {}.{}). Adaptador: {}.", (static_cast<uint32_t>(selected_level) >> 12) & 0xF, (static_cast<uint32_t>(selected_level) >> 8) & 0xF, name);

        m_valid = true;
    }

    DX11Device::~DX11Device() { if (m_imm_ctx) m_imm_ctx->Flush(); }

    // Creación de buffers ----------------------------------------------------------------------------
    anxiety::rendering::rhi::BufferHandle DX11Device::create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz) {
        // TODO: implementar la asignación de buffers D3D11 committed (tipo de heap según el uso).
        if (!m_valid || desc.size_bytes == 0) return {};

        // Los constant buffers deben ser múltiplo de 16 bytes.
        const bool     is_cbv       = has_flag(desc.usage, rhi::BufferUsage::Uniform);
        const uint64_t aligned_size = is_cbv ? (desc.size_bytes + 15u) & ~uint64_t{15u} : desc.size_bytes;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth      = static_cast<UINT>(aligned_size);
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = 0;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (has_flag(desc.usage, rhi::BufferUsage::Vertex))  bd.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
        if (has_flag(desc.usage, rhi::BufferUsage::Index))   bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
        if (has_flag(desc.usage, rhi::BufferUsage::Uniform)) bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
        if (has_flag(desc.usage, rhi::BufferUsage::Storage)) {
            // UAV — usa uso DEFAULT para permitir escritura desde cómputo
            bd.Usage          = D3D11_USAGE_DEFAULT;
            bd.BindFlags     |= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            bd.CPUAccessFlags = 0;
        }
        if (has_flag(desc.usage, rhi::BufferUsage::Indirect)) bd.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

        D3D11_SUBRESOURCE_DATA init_sd{};
        init_sd.pSysMem = initial_data;

        ComPtr<ID3D11Buffer> buf;
        HRESULT hr = m_device->CreateBuffer(&bd, (initial_data && initial_data_sz > 0) ? &init_sd : nullptr, &buf);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "create_buffer: CreateBuffer falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        const uint32_t slot = allocate_buffer_slot();
        auto& s             = m_buffers[slot];
        s.buffer            = std::move(buf);
        s.size_bytes        = aligned_size;
        s.alive             = true;

        return { static_cast<uint64_t>(slot) + 1u };
    }

    rhi::TextureHandle DX11Device::create_texture(const rhi::TextureDesc& desc) {
        if (!m_valid) return {};

        DXGI_FORMAT resource_fmt;
        DXGI_FORMAT dsv_fmt = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT srv_fmt;

        if (desc.is_depth_target) {
            // Usa un formato de recurso "typeless" para poder crear tanto una DSV como una SRV a partir de la misma textura.
            switch (desc.format) {
            case rhi::Format::D24_Unorm_S8_Uint:
                resource_fmt = DXGI_FORMAT_R24G8_TYPELESS;
                dsv_fmt      = DXGI_FORMAT_D24_UNORM_S8_UINT;
                srv_fmt      = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                break;
            case rhi::Format::D32_Float:
            default:
                resource_fmt = DXGI_FORMAT_R32_TYPELESS;
                dsv_fmt      = DXGI_FORMAT_D32_FLOAT;
                srv_fmt      = DXGI_FORMAT_R32_FLOAT;
                break;
            }
        } else {
            resource_fmt = to_DX11_format(desc.format);
            srv_fmt      = resource_fmt;
        }

        D3D11_TEXTURE2D_DESC td{};
        td.Width      = desc.extent.width;
        td.Height     = desc.extent.height;
        td.MipLevels  = desc.mip_levels;
        td.ArraySize  = desc.array_size;
        td.Format     = resource_fmt;
        td.SampleDesc = { 1, 0 };
        td.Usage      = D3D11_USAGE_DEFAULT;
        td.BindFlags  = D3D11_BIND_SHADER_RESOURCE;
        if (desc.is_render_target)  td.BindFlags |= D3D11_BIND_RENDER_TARGET;
        if (desc.is_depth_target)   td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        if (desc.is_storage_target) td.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = m_device->CreateTexture2D(&td, nullptr, &tex);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "create_texture: CreateTexture2D falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.texture = std::move(tex);
        s.width   = desc.extent.width;
        s.height  = desc.extent.height;
        s.format  = resource_fmt;
        s.alive   = true;

        if (desc.is_depth_target) {
            s.is_depth = true;
            s.state    = rhi::ResourceState::DepthWrite;

            D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
            dsv_desc.Format             = dsv_fmt;
            dsv_desc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Texture2D.MipSlice = 0;
            m_device->CreateDepthStencilView(s.texture.Get(), &dsv_desc, &s.dsv);

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format                    = srv_fmt;
            srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels       = desc.mip_levels;
            srv_desc.Texture2D.MostDetailedMip = 0;
            m_device->CreateShaderResourceView(s.texture.Get(), &srv_desc, &s.srv);
        } else {
            s.state = rhi::ResourceState::Undefined;

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format                    = srv_fmt;
            srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels       = desc.mip_levels;
            srv_desc.Texture2D.MostDetailedMip = 0;
            m_device->CreateShaderResourceView(s.texture.Get(), &srv_desc, &s.srv);

            if (desc.is_render_target) {
                s.state = rhi::ResourceState::RenderTarget;
                m_device->CreateRenderTargetView(s.texture.Get(), nullptr, &s.rtv);
            }
        }

        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX11Device::destroy_buffer(rhi::BufferHandle handle) {
        if (!handle.is_valid()) return;
        free_buffer_slot(to_buffer_slot(handle));
    }

    void DX11Device::destroy_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    // Escrituras de buffer -----------------------------------------------------------------------
    void DX11Device::write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) {
        ID3D11Buffer* buf = lookup_buffer(handle);
        if (!buf || !data || size == 0) return;

        // MAP_WRITE_DISCARD reemplaza el buffer completo; el offset se aplica del lado de la CPU.
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = m_imm_ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            LOG_WARNING(k_category, "write_buffer: Map falló.");
            return;
        }
        std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
        m_imm_ctx->Unmap(buf, 0);
    }

    // Carga de textura ----------------------------------------------------------------------------
    void DX11Device::upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) {
        if (!handle.is_valid() || !rgba8 || width == 0 || height == 0) return;
        const uint32_t slot = to_texture_slot(handle);
        if (slot >= m_textures.size() || !m_textures[slot].alive) return;

        // UpdateSubresource de DX11 no requiere un staging buffer para texturas DEFAULT.
        m_imm_ctx->UpdateSubresource(m_textures[slot].texture.Get(), 0, nullptr, rgba8,
            width * 4u,   // pitch de fila (RGBA8 = 4 bytes/píxel)
            0);
    }

    // Compilación de shaders ----------------------------------------------------------------------
    std::vector<uint8_t> DX11Device::compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) {
        // Los perfiles de destino de DX11 usan shader model 5.0.
        const char* profile = nullptr;
        switch (stage) {
        case rhi::ShaderStage::Vertex:   profile = "vs_5_0"; break;
        case rhi::ShaderStage::Fragment: profile = "ps_5_0"; break;
        case rhi::ShaderStage::Compute:  profile = "cs_5_0"; break;
        default: return {};
        }

        ComPtr<ID3DBlob> blob, error_blob;
        const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
            entry_point, profile,
            D3DCOMPILE_OPTIMIZATION_LEVEL0, 0,
            &blob, &error_blob);

        if (FAILED(hr)) {
            if (error_blob)
                LOGF_ERROR(k_category, "Error de compilación de shader: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
            return {};
        }

        const auto* data = static_cast<const uint8_t*>(blob->GetBufferPointer());
        return { data, data + blob->GetBufferSize() };
    }

    // Fábricas de shader / pipeline / descriptor set ------------------------------------------------
    std::unique_ptr<rhi::IShader> DX11Device::create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) {
        if (!m_valid) return nullptr;
        auto shader = std::make_unique<DX11Shader>(m_device.Get(), desc, stage);
        if (!shader->is_valid()) {
            LOG_ERROR(k_category, "create_shader: la compilación falló.");
            return nullptr;
        }
        return shader;
    }

    std::unique_ptr<rhi::IPipeline> DX11Device::create_pipeline(const rhi::PipelineDesc& desc) {
        if (!m_valid) return nullptr;
        auto pipeline = std::make_unique<DX11Pipeline>(m_device.Get(), desc);
        if (!pipeline->is_valid()) {
            LOG_ERROR(k_category, "create_pipeline: falló la creación del PSO.");
            return nullptr;
        }
        return pipeline;
    }

    std::unique_ptr<rhi::IDescriptorSet> DX11Device::create_descriptor_set(const rhi::DescriptorSetLayout& layout) {
        if (!m_valid) return nullptr;
        return std::make_unique<DX11DescriptorSet>(*this, layout);
    }

    // Command buffer -----------------------------------------------------------------------------
    std::unique_ptr<rhi::ICommandBuffer> DX11Device::create_command_buffer() {
        if (!m_valid) return nullptr;

        ComPtr<ID3D11DeviceContext> deferred;
        HRESULT hr = m_device->CreateDeferredContext(0, &deferred);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateDeferredContext falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return nullptr;
        }
        return std::make_unique<DX11CommandBuffer>(*this, std::move(deferred));
    }

    // Swapchain ----------------------------------------------------------------------------------
    std::unique_ptr<rhi::ISwapchain> DX11Device::create_swapchain(const rhi::SwapchainDesc& desc) {
        if (!m_valid || !desc.native_window_handle) return nullptr;
        auto sc = std::make_unique<DX11Swapchain>(*this, desc);
        if (!sc->is_valid()) return nullptr;
        return sc;
    }

    // Envío y sincronización ------------------------------------------------------------------------------
    void DX11Device::submit(rhi::ICommandBuffer& cmd) {
        if (!m_valid) return;
        auto& dx11_cmd = static_cast<DX11CommandBuffer&>(cmd);
        if (ID3D11CommandList* list = dx11_cmd.command_list()) m_imm_ctx->ExecuteCommandList(list, FALSE);
    }

    void DX11Device::wait_idle() {
        if (!m_valid) return;
        // Usa un D3D11_QUERY_EVENT para bloquear hasta que la GPU vacíe la cola de comandos.
        D3D11_QUERY_DESC qd{ D3D11_QUERY_EVENT, 0 };
        ComPtr<ID3D11Query> q;
        if (FAILED(m_device->CreateQuery(&qd, &q))) {
            m_imm_ctx->Flush();
            return;
        }
        m_imm_ctx->End(q.Get());
        BOOL done = FALSE;
        while (m_imm_ctx->GetData(q.Get(), &done, sizeof(done), 0) == S_FALSE || !done) {}
    }

    // Registro de texturas externas --------------------------------------------------------------
    anxiety::rendering::rhi::TextureHandle DX11Device::register_external_texture(ID3D11Texture2D* texture, rhi::ResourceState state) {
        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.texture = texture;           // AddRef mediante la asignación de ComPtr
        s.state   = state;
        s.alive   = true;

        D3D11_TEXTURE2D_DESC td{};
        texture->GetDesc(&td);
        s.width  = td.Width;
        s.height = td.Height;
        s.format = td.Format;

        // Los backbuffers necesitan un RTV para que el command buffer pueda limpiarlos / renderizar en ellos.
        if (td.BindFlags & D3D11_BIND_RENDER_TARGET) m_device->CreateRenderTargetView(s.texture.Get(), nullptr, &s.rtv);

        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX11Device::unregister_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    // Búsquedas de recursos ----------------------------------------------------------------------
    ID3D11RenderTargetView* DX11Device::lookup_RTV(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return nullptr;
        return m_textures[s].rtv.Get();
    }

    ID3D11DepthStencilView* DX11Device::lookup_DSV(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return nullptr;
        return m_textures[s].dsv.Get();
    }

    ID3D11ShaderResourceView* DX11Device::lookup_SRV(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return nullptr;
        return m_textures[s].srv.Get();
    }

    std::pair<uint32_t, uint32_t> DX11Device::texture_extent(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return { 0, 0 };
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return { 0, 0 };
        return { m_textures[s].width, m_textures[s].height };
    }

    ID3D11Buffer* DX11Device::lookup_buffer(rhi::BufferHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_buffer_slot(h);
        if (s >= m_buffers.size() || !m_buffers[s].alive) return nullptr;
        return m_buffers[s].buffer.Get();
    }

    // Gestión del pool ----------------------------------------------------------------------------
    uint32_t DX11Device::allocate_texture_slot() {
        if (!m_texture_free_list.empty()) {
            const uint32_t s = m_texture_free_list.back();
            m_texture_free_list.pop_back();
            return s;
        }
        const uint32_t s = static_cast<uint32_t>(m_textures.size());
        m_textures.emplace_back();
        return s;
    }

    void DX11Device::free_texture_slot(uint32_t slot) {
        assert(slot < m_textures.size());
        m_textures[slot] = TextureSlot{};
        m_texture_free_list.push_back(slot);
    }

    uint32_t DX11Device::allocate_buffer_slot() {
        if (!m_buffer_free_list.empty()) {
            const uint32_t s = m_buffer_free_list.back();
            m_buffer_free_list.pop_back();
            return s;
        }
        const uint32_t s = static_cast<uint32_t>(m_buffers.size());
        m_buffers.emplace_back();
        return s;
    }

    void DX11Device::free_buffer_slot(uint32_t slot) {
        assert(slot < m_buffers.size());
        m_buffers[slot] = BufferSlot{};
        m_buffer_free_list.push_back(slot);
    }
} // namespace anxiety::rendering::backend::dx11

#endif ANXIETY_BACKEND_DX11