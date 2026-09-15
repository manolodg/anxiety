#include "DX11Device.h"
#include "DX11CommandBuffer.h"
#include "DX11Swapchain.h"
#include "DX11Helpers.h"
#include "Logger.h"

#include <cassert>
#include <algorithm>

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    static inline uint32_t to_texture_slot(anxiety::rendering::rhi::TextureHandle h) noexcept { return static_cast<uint32_t>(h.id) - 1u; }

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
    anxiety::rendering::rhi::BufferHandle DX11Device::create_buffer(const anxiety::rendering::rhi::BufferDesc& /*desc*/) {
        // TODO: implementar la asignación de buffers D3D11 committed (tipo de heap según el uso).
        LOG_WARNING(k_category, "DX11Device::createBuffer — todavía no implementado.");
        return {};
    }

    anxiety::rendering::rhi::TextureHandle DX11Device::create_texture(const anxiety::rendering::rhi::TextureDesc& desc) {
        if (!m_valid) return {};

        D3D11_TEXTURE2D_DESC td{};
        td.Width      = desc.extent.width;
        td.Height     = desc.extent.height;
        td.MipLevels  = desc.mip_levels;
        td.ArraySize  = desc.array_size;
        td.Format     = to_DX11_format(desc.format);
        td.SampleDesc = { 1, 0 };
        td.Usage      = D3D11_USAGE_DEFAULT;
        td.BindFlags  = D3D11_BIND_SHADER_RESOURCE;
        if (desc.is_render_target)  td.BindFlags |= D3D11_BIND_RENDER_TARGET;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = m_device->CreateTexture2D(&td, nullptr, &tex);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "createTexture: CreateTexture2D falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.texture = std::move(tex);
        s.width   = desc.extent.width;
        s.height  = desc.extent.height;
        s.format  = td.Format;
        s.alive   = true;

        s.state = anxiety::rendering::rhi::ResourceState::Undefined;

        if (desc.is_render_target) {
            s.state = anxiety::rendering::rhi::ResourceState::RenderTarget;
            m_device->CreateRenderTargetView(s.texture.Get(), nullptr, &s.rtv);
        }

        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX11Device::destroy_buffer(anxiety::rendering::rhi::BufferHandle /*handle*/) {
        // TODO: liberar el slot del buffer en el pool.
    }

    void DX11Device::destroy_texture(anxiety::rendering::rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    // Command buffer -----------------------------------------------------------------------------
    std::unique_ptr<anxiety::rendering::rhi::ICommandBuffer> DX11Device::create_command_buffer() {
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
    std::unique_ptr<anxiety::rendering::rhi::ISwapchain> DX11Device::create_swapchain(const anxiety::rendering::rhi::SwapchainDesc& desc) {
        if (!m_valid || !desc.native_window_handle) return nullptr;
        auto sc = std::make_unique<DX11Swapchain>(*this, desc);
        if (!sc->is_valid()) return nullptr;
        return sc;
    }

    // Envío y sincronización ------------------------------------------------------------------------------
    void DX11Device::submit(anxiety::rendering::rhi::ICommandBuffer& cmd) {
        if (!m_valid) return;
        auto& dx11Cmd = static_cast<DX11CommandBuffer&>(cmd);
        if (ID3D11CommandList* list = dx11Cmd.command_list()) m_imm_ctx->ExecuteCommandList(list, FALSE);
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
    anxiety::rendering::rhi::TextureHandle DX11Device::register_external_texture(ID3D11Texture2D* texture, anxiety::rendering::rhi::ResourceState state) {
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

    void DX11Device::unregister_texture(anxiety::rendering::rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    // Consultas de recursos ---------------------------------------------------------------------------

    ID3D11RenderTargetView* DX11Device::lookup_RTV(anxiety::rendering::rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return nullptr;
        return m_textures[s].rtv.Get();
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
} // namespace anxiety::rendering::backend::dx11