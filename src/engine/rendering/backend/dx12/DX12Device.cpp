#ifdef ANXIETY_BACKEND_DX12

#include "DX12Device.h"
#include "DX12CommandBuffer.h"
#include "DX12Swapchain.h"
#include "DX12Shader.h"
#include "DX12Pipeline.h"
#include "DX12DescriptorSet.h"
#include "DX12Helpers.h"
#include "Logger.h"

#include <algorithm>
#include <cassert>
#include <d3dcompiler.h>

namespace anxiety::rendering::backend::dx12 {
    // Helpers ------------------------------------------------------------------------------------
    static constexpr char k_category[] = "RHI";

    static inline uint32_t to_texture_slot(rhi::TextureHandle h) noexcept { return static_cast<uint32_t>(h.id) - 1u; }
    static inline uint32_t to_buffer_slot(rhi::BufferHandle h)   noexcept { return static_cast<uint32_t>(h.id) - 1u; }

    // Construcción / destrucción --------------------------------------------------------------------
    DX12Device::DX12Device(bool enable_debug_layer) {
        // Capa de depuración opcional (requiere una característica opcional del Windows SDK).
        if (enable_debug_layer) {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
                debug->EnableDebugLayer();
                LOG_INFO(k_category, "Capa de depuración de D3D12 activada.");
            }
            else {
                LOG_WARNING(k_category, "Se solicitó la capa de depuración de D3D12 pero no está disponible.");
            }
        }

        // Fábrica de DXGI.
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateDXGIFactory2 falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Enumera los adaptadores de hardware; omite los de software.
        for (UINT i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> candidate;
            if (m_factory->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND) break;

            DXGI_ADAPTER_DESC1 desc{};
            candidate->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

            // Verifica que D3D12 esté soportado sin llegar a crear el dispositivo.
            if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                m_adapter = candidate;
                break;
            }
        }

        // Recurre al rasterizador de software WARP si no se encontró ningún adaptador de hardware.
        if (!m_adapter) {
            hr = m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter));
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "No se encontró ningún adaptador D3D12 adecuado: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }

            LOG_INFO(k_category, "No se encontró ninguna GPU de hardware — usando el adaptador de software WARP.");
        }

        // Crea el dispositivo D3D12.
        hr = D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "D3D12CreateDevice falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Cola de comandos directa / de gráficos.
        D3D12_COMMAND_QUEUE_DESC qDesc{};
        qDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
        qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        hr = m_device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&m_cmd_queue));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommandQueue falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Fence para waitIdle().
        hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_idle_fence));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateFence falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }
        m_idle_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_idle_event) {
            LOG_ERROR(k_category, "CreateEvent falló.");
            return;
        }

        // Heap de descriptores RTV no visible desde el shader (solo del lado de la CPU).
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = k_rtv_heap_size;
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtv_heap));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateDescriptorHeap(RTV) falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }
        m_rtv_desc_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Heap de DSV no visible desde el shader
        {
            D3D12_DESCRIPTOR_HEAP_DESC d{};
            d.NumDescriptors = k_dsv_heap_size;
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = m_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_dsv_heap));
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateDescriptorHeap(DSV) falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
            m_dsv_desc_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        }

        // Heap SRV/CBV/UAV visible desde el shader
        {
            D3D12_DESCRIPTOR_HEAP_DESC d{};
            d.NumDescriptors = k_srv_heap_size;
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = m_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_srv_heap));
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateDescriptorHeap(SRV) falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }

            m_srv_desc_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // Registra en el log el nombre del adaptador.
        DXGI_ADAPTER_DESC1 adapter_desc{};
        m_adapter->GetDesc1(&adapter_desc);
        char adapter_name[128]{};
        WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, adapter_name, static_cast<int>(sizeof(adapter_name)), nullptr, nullptr);
        LOGF_INFO(k_category, "Dispositivo DirectX 12 listo. Adaptador: {}.", adapter_name);

        m_valid = true;
    }

    DX12Device::~DX12Device() {
        if (m_idle_event) {
            wait_idle();
            CloseHandle(m_idle_event);
            m_idle_event = nullptr;
        }
    }

    // Creación de buffers --------------------------------------------------------------------------
    rhi::BufferHandle DX12Device::create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz) {
        if (!m_valid || desc.size_bytes == 0) return {};

        // Los constant buffers deben estar alineados a 256 bytes en D3D12.
        const bool is_CBV = (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(rhi::BufferUsage::Uniform)) != 0;
        const uint64_t aligned_size = is_CBV ? (desc.size_bytes + 255) & ~uint64_t{ 255 } : desc.size_bytes;

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;   // escribible desde CPU, legible desde GPU (suficiente para la demo)

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Alignment        = 0;
        rd.Width            = aligned_size;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> resource;
        HRESULT hr = m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "createBuffer: CreateCommittedResource falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        // Sube los datos iniciales
        if (initial_data && initial_data_sz > 0) {
            void* mapped = nullptr;
            D3D12_RANGE readRange{ 0, 0 };
            if (SUCCEEDED(resource->Map(0, &readRange, &mapped))) {
                std::memcpy(mapped, initial_data, std::min(initial_data_sz, static_cast<size_t>(aligned_size)));
                resource->Unmap(0, nullptr);
            }
        }

        const uint32_t slot = allocate_buffer_slot();
        auto& s = m_buffers[slot];
        s.resource = std::move(resource);
        s.gpu_VA = s.resource->GetGPUVirtualAddress();
        s.size_bytes = aligned_size;
        s.alive = true;

        return { static_cast<uint64_t>(slot) + 1u };
    }

    rhi::TextureHandle DX12Device::create_texture(const rhi::TextureDesc& desc) {
        if (!m_valid) return {};

        const bool is_depth = (desc.format == rhi::Format::D32_Float || desc.format == rhi::Format::D24_Unorm_S8_Uint);

        D3D12_RESOURCE_DESC d{};
        d.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width            = desc.extent.width;
        d.Height           = desc.extent.height;
        d.DepthOrArraySize = static_cast<UINT16>(desc.array_size);
        d.MipLevels        = static_cast<UINT16>(desc.mip_levels);
        d.Format           = to_D3D12_format(desc.format);
        d.SampleDesc       = { 1, 0 };
        d.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        d.Flags            = is_depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : desc.is_render_target ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;

        const D3D12_RESOURCE_STATES init_state = is_depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : desc.is_render_target ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COMMON;

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearVal{};
        clearVal.Format = d.Format;
        if (is_depth) { 
            clearVal.DepthStencil.Depth   = 1.0f;
            clearVal.DepthStencil.Stencil = 0; 
        }

        const bool needs_clear_val = desc.is_render_target || is_depth;

        ComPtr<ID3D12Resource> resource;
        HRESULT hr = m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, init_state, needs_clear_val ? &clearVal : nullptr, IID_PPV_ARGS(&resource));

        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "create_texture falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.resource    = std::move(resource);
        s.width       = desc.extent.width;
        s.height      = desc.extent.height;
        s.dxgi_format = d.Format;
        s.state       = is_depth ? rhi::ResourceState::DepthWrite : desc.is_render_target ? rhi::ResourceState::RenderTarget : rhi::ResourceState::Undefined;
        s.alive       = true;

        if (desc.is_render_target) {
            s.rtv_handle = allocate_RTV();
            m_device->CreateRenderTargetView(s.resource.Get(), nullptr, s.rtv_handle);
            s.has_RTV = true;
        }

        if (is_depth) {
            s.dsv_handle = allocate_DSV();
            m_device->CreateDepthStencilView(s.resource.Get(), nullptr, s.dsv_handle);
            s.has_DSV = true;
        }

        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX12Device::destroy_buffer(rhi::BufferHandle handle) {
        if (!handle.is_valid()) return;
        free_buffer_slot(to_buffer_slot(handle));
    }

    void DX12Device::destroy_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    void DX12Device::write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) {
        ID3D12Resource* res = lookup_buffer(handle);
        if (!res || !data || size == 0) return;

        void* mapped = nullptr;
        D3D12_RANGE readRange{ 0, 0 };          // no se lee de vuelta desde la CPU
        if (FAILED(res->Map(0, &readRange, &mapped))) {
            LOG_WARNING(k_category, "writeBuffer: Map falló.");
            return;
        }
        memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
        D3D12_RANGE writeRange{ offset, offset + size };
        res->Unmap(0, &writeRange);
    }

    void DX12Device::upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) {
        if (!handle.is_valid() || !rgba8 || width == 0 || height == 0) return;
        const uint32_t tSlot = to_texture_slot(handle);
        if (tSlot >= m_textures.size() || !m_textures[tSlot].alive) return;
        auto& s = m_textures[tSlot];

        // El row pitch debe ser múltiplo de D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256).
        const uint32_t srcRowBytes = width * 4u;  // RGBA8: 4 bytes/píxel
        const uint32_t alignedRowBytes = (srcRowBytes + 255u) & ~255u;
        const uint64_t uploadBytes = static_cast<uint64_t>(alignedRowBytes) * height;

        // Buffer de staging para la subida -----------------------------------------------------
        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width            = uploadBytes;
        bufDesc.Height           = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels        = 1;
        bufDesc.Format           = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc       = { 1, 0 };
        bufDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> uploadBuf;
        HRESULT hr = m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "upload_texture_data: falló la creación del upload buffer: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Copia las filas de píxeles, rellenando cada una hasta alignedRowBytes.
        void* mapped = nullptr;
        uploadBuf->Map(0, nullptr, &mapped);
        auto* dst = static_cast<uint8_t*>(mapped);
        for (uint32_t row = 0; row < height; ++row) {
            memcpy(dst + row * alignedRowBytes, rgba8 + row * srcRowBytes, srcRowBytes);
        }
        uploadBuf->Unmap(0, nullptr);

        // Command list de un solo uso --------------------------------------------------------
        ComPtr<ID3D12CommandAllocator> alloc;
        hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
        if (FAILED(hr)) { LOGF_ERROR(k_category, "upload_texture_data: CreateCommandAllocator falló: 0x{:08X}", static_cast<uint32_t>(hr)); return; }

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) { LOGF_ERROR(k_category, "upload_texture_data: CreateCommandList falló: 0x{:08X}", static_cast<uint32_t>(hr)); return; }

        // Transición de la textura: COMMON → COPY_DEST
        {
            D3D12_RESOURCE_BARRIER bar{};
            bar.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            bar.Transition.pResource   = s.resource.Get();
            bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            bar.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
            bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmdList->ResourceBarrier(1, &bar);
        }

        // CopyTextureRegion: buffer → subrecurso 0 de la textura
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        footprint.Offset             = 0;
        footprint.Footprint.Format   = s.dxgi_format;
        footprint.Footprint.Width    = width;
        footprint.Footprint.Height   = height;
        footprint.Footprint.Depth    = 1;
        footprint.Footprint.RowPitch = alignedRowBytes;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource       = uploadBuf.Get();
        srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = s.resource.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        // Transición: COPY_DEST → PIXEL_SHADER_RESOURCE
        {
            D3D12_RESOURCE_BARRIER bar{};
            bar.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            bar.Transition.pResource   = s.resource.Get();
            bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            bar.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmdList->ResourceBarrier(1, &bar);
        }

        cmdList->Close();

        ID3D12CommandList* lists[] = { cmdList.Get() };
        m_cmd_queue->ExecuteCommandLists(1, lists);

        wait_idle();  // bloquea hasta que la copia en GPU termina

        s.state = anxiety::rendering::rhi::ResourceState::ShaderResource;
    }

    // Compilación de shaders -------------------------------------------------------------------------
    std::vector<uint8_t> DX12Device::compile_shader_from_source(const char* source, const char* entry_point, anxiety::rendering::rhi::ShaderStage stage) {
        const char* profile = nullptr;
        switch (stage) {
        case rhi::ShaderStage::Vertex:   profile = "vs_5_1"; break;
        case rhi::ShaderStage::Fragment: profile = "ps_5_1"; break;
        case rhi::ShaderStage::Compute:  profile = "cs_5_1"; break;
        default: return {};
        }

        ComPtr<ID3DBlob> blob, error_blob;
        const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry_point, profile, D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, &blob, &error_blob);

        if (FAILED(hr)) {
            if (error_blob) LOGF_ERROR(k_category, "Error de compilación de shader: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
            return {};
        }

        const auto* data = static_cast<const uint8_t*>(blob->GetBufferPointer());
        return { data, data + blob->GetBufferSize() };
    }

    // Fábricas de shaders / pipelines / descriptor sets -----------------------------------------------
    std::unique_ptr<rhi::IShader> DX12Device::create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) {
        if (!m_valid) return nullptr;
        auto shader = std::make_unique<DX12Shader>(desc, stage);
        if (!shader->is_valid()) {
            LOG_ERROR(k_category, "createShader: bytecode vacío.");
            return nullptr;
        }
        return shader;
    }

    std::unique_ptr<rhi::IPipeline> DX12Device::create_pipeline(const rhi::PipelineDesc& desc) {
        if (!m_valid) return nullptr;
        auto pipeline = std::make_unique<DX12Pipeline>(m_device.Get(), desc);
        if (!pipeline->is_valid()) {
            LOG_ERROR(k_category, "createPipeline: falló la creación del PSO.");
            return nullptr;
        }
        return pipeline;
    }

    std::unique_ptr<rhi::IDescriptorSet> DX12Device::create_descriptor_set(const rhi::DescriptorSetLayout& layout) {
        if (!m_valid) return nullptr;
        return std::make_unique<DX12DescriptorSet>(*this, layout);
    }

    // Command buffer / swapchain -----------------------------------------------------------------
    std::unique_ptr<rhi::ICommandBuffer> DX12Device::create_command_buffer() {
        if (!m_valid) return nullptr;

        ComPtr<ID3D12CommandAllocator> allocator;
        HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommandAllocator falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return nullptr;
        }

        ComPtr<ID3D12GraphicsCommandList> cmd_list;
        hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommandList falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return nullptr;
        }

        // Se cierra de inmediato — DX12CommandBuffer::begin() lo abre antes de grabar.
        cmd_list->Close();

        return std::make_unique<DX12CommandBuffer>(*this, std::move(allocator), std::move(cmd_list));
    }

    std::unique_ptr<rhi::ISwapchain> DX12Device::create_swapchain(const rhi::SwapchainDesc& desc) {
        if (!m_valid || !desc.native_window_handle) return nullptr;
        return std::make_unique<DX12Swapchain>(*this, desc);
    }

    // Envío y sincronización -------------------------------------------------------------------
    void DX12Device::submit(rhi::ICommandBuffer& cmd) {
        if (!m_valid) return;
        auto& dx12 = static_cast<DX12CommandBuffer&>(cmd);
        ID3D12CommandList* lists[] = { dx12.native_list() };
        m_cmd_queue->ExecuteCommandLists(1, lists);
    }

    void DX12Device::wait_idle() {
        if (!m_valid || !m_cmd_queue || !m_idle_fence) return;

        const uint64_t val = ++m_idle_fence_value;
        m_cmd_queue->Signal(m_idle_fence.Get(), val);
        if (m_idle_fence->GetCompletedValue() < val) {
            m_idle_fence->SetEventOnCompletion(val, m_idle_event);
            WaitForSingleObject(m_idle_event, INFINITE);
        }
    }

    // Asignador de RTV -----------------------------------------------------------------------------
    D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::allocate_RTV() {
        uint32_t slot;
        if (!m_rtv_free_list.empty()) {
            // Reutiliza un slot liberado antes de crecer — sin esto, cada resize()/recreate() del
            // swapchain (o de un render target) agotaba el heap tras k_rtv_heap_size ciclos.
            slot = m_rtv_free_list.back();
            m_rtv_free_list.pop_back();
        } else {
            assert(m_rtv_next_slot < k_rtv_heap_size && "Heap de RTV agotado");
            slot = m_rtv_next_slot++;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(slot) * m_rtv_desc_size;
        return h;
    }

    void DX12Device::free_RTV(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        if (!handle.ptr || !m_rtv_heap) return;

        const SIZE_T base = m_rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
        const uint32_t slot = static_cast<uint32_t>((handle.ptr - base) / m_rtv_desc_size);
        m_rtv_free_list.push_back(slot);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::allocate_DSV() {
        assert(m_dsv_next_slot < k_dsv_heap_size && "Heap de DSV agotado");
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(m_dsv_next_slot) * m_dsv_desc_size;
        ++m_dsv_next_slot;
        return h;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::texture_DSV(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return { 0 };
        const uint32_t slot = to_texture_slot(h);
        if (slot >= m_textures.size() || !m_textures[slot].has_DSV) return { 0 };
        return m_textures[slot].dsv_handle;
    }

    SRVDescriptor DX12Device::allocate_SRV_descriptor() {
        if (m_srv_next_slot >= k_srv_heap_size) return {};
        SRVDescriptor alloc;
        alloc.cpu      = m_srv_heap->GetCPUDescriptorHandleForHeapStart();
        alloc.gpu      = m_srv_heap->GetGPUDescriptorHandleForHeapStart();
        alloc.cpu.ptr += static_cast<SIZE_T>(m_srv_next_slot) * m_srv_desc_size;
        alloc.gpu.ptr += static_cast<SIZE_T>(m_srv_next_slot) * m_srv_desc_size;
        ++m_srv_next_slot;
        return alloc;
    }

    // Registro de texturas externas --------------------------------------------------------------
    rhi::TextureHandle DX12Device::register_external_texture(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtv, rhi::ResourceState state) {
        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.resource   = resource;                     // ComPtr AddRef
        s.rtv_handle = rtv;
        s.has_RTV    = true;
        s.state      = state;
        s.alive      = true;
        // Lee automáticamente las dimensiones del propio desc del recurso D3D12
        const auto rd = resource->GetDesc();
        s.width      = static_cast<uint32_t>(rd.Width);
        s.height     = rd.Height;
        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX12Device::unregister_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_texture_slot(handle));
    }

    // Pool de texturas -------------------------------------------------------------------------------
    uint32_t DX12Device::allocate_texture_slot() {
        if (!m_texture_free_list.empty()) {
            const uint32_t s = m_texture_free_list.back();
            m_texture_free_list.pop_back();
            return s;
        }
        const uint32_t s = static_cast<uint32_t>(m_textures.size());
        m_textures.emplace_back();
        return s;
    }

    void DX12Device::free_texture_slot(uint32_t slot) {
        assert(slot < m_textures.size());
        // Libera también el RTV asociado (si lo tenía) antes de reiniciar el slot — esto es lo que
        // hace que DX12Swapchain::release_backbuffers() -> unregister_texture() ya no filtre RTVs
        // en cada resize(): pasa por aquí mismo.
        if (m_textures[slot].has_RTV) free_RTV(m_textures[slot].rtv_handle);
        m_textures[slot] = TextureSlot{};           // libera el ComPtr, reinicia todos los campos
        m_texture_free_list.push_back(slot);
    }

    // Pool de buffers --------------------------------------------------------------------------------
    uint32_t DX12Device::allocate_buffer_slot() {
        if (!m_buffer_free_list.empty()) {
            const uint32_t s = m_buffer_free_list.back();
            m_buffer_free_list.pop_back();
            return s;
        }
        const uint32_t s = static_cast<uint32_t>(m_buffers.size());
        m_buffers.emplace_back();
        return s;
    }

    void DX12Device::free_buffer_slot(uint32_t slot) {
        assert(slot < m_buffers.size());
        m_buffers[slot] = BufferSlot{};
        m_buffer_free_list.push_back(slot);
    }

    // Consultas de recursos ---------------------------------------------------------------------
    ID3D12Resource* DX12Device::lookup_texture(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t slot = to_texture_slot(h);
        if (slot >= m_textures.size() || !m_textures[slot].alive) return nullptr;
        return m_textures[slot].resource.Get();
    }

    rhi::ResourceState DX12Device::texture_state(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return rhi::ResourceState::Undefined;
        const uint32_t slot = to_texture_slot(h);
        if (slot >= m_textures.size()) return rhi::ResourceState::Undefined;
        return m_textures[slot].state;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::texture_RTV(rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return { 0 };
        const uint32_t slot = to_texture_slot(h);
        if (slot >= m_textures.size() || !m_textures[slot].has_RTV) return { 0 };
        return m_textures[slot].rtv_handle;
    }

    std::pair<uint32_t, uint32_t> DX12Device::texture_extent(anxiety::rendering::rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return { 0, 0 };
        const uint32_t s = to_texture_slot(h);
        if (s >= m_textures.size() || !m_textures[s].alive) return { 0, 0 };
        return { m_textures[s].width, m_textures[s].height };
    }

    void DX12Device::set_texture_state(rhi::TextureHandle h, rhi::ResourceState new_state) {
        if (!h.is_valid()) return;
        const uint32_t slot = to_texture_slot(h);
        if (slot < m_textures.size()) m_textures[slot].state = new_state;
    }

    ID3D12Resource* DX12Device::lookup_buffer(anxiety::rendering::rhi::BufferHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t s = to_buffer_slot(h);
        if (s >= m_buffers.size() || !m_buffers[s].alive) return nullptr;
        return m_buffers[s].resource.Get();
    }

    D3D12_GPU_VIRTUAL_ADDRESS DX12Device::buffer_GPU_VA(anxiety::rendering::rhi::BufferHandle h) const noexcept {
        if (!h.is_valid()) return 0;
        const uint32_t s = to_buffer_slot(h);
        if (s >= m_buffers.size() || !m_buffers[s].alive) return 0;
        return m_buffers[s].gpu_VA;
    }

    uint64_t DX12Device::buffer_size(anxiety::rendering::rhi::BufferHandle h) const noexcept {
        if (!h.is_valid()) return 0;
        const uint32_t s = to_buffer_slot(h);
        if (s >= m_buffers.size() || !m_buffers[s].alive) return 0;
        return m_buffers[s].size_bytes;
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12