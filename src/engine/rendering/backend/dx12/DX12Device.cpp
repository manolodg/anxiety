#include "DX12Device.h"
#include "DX12CommandBuffer.h"
#include "DX12Swapchain.h"
#include "DX12Helpers.h"
#include "Logger.h"

#include <cassert>

namespace anxiety::rendering::backend::dx12 {
    // Helpers ------------------------------------------------------------------------------------
    static constexpr char k_category[] = "RHI";

    static inline uint32_t to_slot(anxiety::rendering::rhi::TextureHandle h) noexcept { return static_cast<uint32_t>(h.id) - 1u; }

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
        qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
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
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtv_heap));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateDescriptorHeap(RTV) falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }
        m_rtv_desc_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Registra en el log el nombre del adaptador.
        DXGI_ADAPTER_DESC1 adapterDesc{};
        m_adapter->GetDesc1(&adapterDesc);
        char adapterName[128]{};
        WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, adapterName, static_cast<int>(sizeof(adapterName)), nullptr, nullptr);
        LOGF_INFO(k_category, "Dispositivo DirectX 12 listo. Adaptador: {}.", adapterName);

        m_valid = true;
    }

    DX12Device::~DX12Device() {
        if (m_idle_event) {
            wait_idle();
            CloseHandle(m_idle_event);
            m_idle_event = nullptr;
        }
    }

    // Creación / destrucción de recursos ---------------------------------------------------------
    anxiety::rendering::rhi::BufferHandle DX12Device::create_buffer(const anxiety::rendering::rhi::BufferDesc& /*desc*/) {
        // TODO: implementar la asignación de buffers D3D12 committed (tipo de heap según el uso).
        LOG_WARNING(k_category, "DX12Device::createBuffer — todavía no implementado.");
        return {};
    }

    anxiety::rendering::rhi::TextureHandle DX12Device::create_texture(const anxiety::rendering::rhi::TextureDesc& desc) {
        if (!m_valid) return {};

        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = desc.extent.width;
        d.Height = desc.extent.height;
        d.DepthOrArraySize = static_cast<UINT16>(desc.array_size);
        d.MipLevels = static_cast<UINT16>(desc.mip_levels);
        d.Format = to_D3D12_format(desc.format);
        d.SampleDesc = { 1, 0 };
        d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        d.Flags = desc.is_render_target ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;

        const D3D12_RESOURCE_STATES initState = desc.is_render_target ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COMMON;

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearVal{};
        clearVal.Format = d.Format;

        ComPtr<ID3D12Resource> resource;
        HRESULT hr = m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, initState, desc.is_render_target ? &clearVal : nullptr, IID_PPV_ARGS(&resource));

        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommittedResource(texture) falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return {};
        }

        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.resource = resource;
        s.state = desc.is_render_target ? anxiety::rendering::rhi::ResourceState::RenderTarget : anxiety::rendering::rhi::ResourceState::Undefined;
        s.alive = true;

        if (desc.is_render_target) {
            s.rtv_handle = allocate_RTV();
            m_device->CreateRenderTargetView(resource.Get(), nullptr, s.rtv_handle);
            s.has_RTV = true;
        }

        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX12Device::destroy_buffer(anxiety::rendering::rhi::BufferHandle /*handle*/) {
        // TODO: liberar el slot del buffer en el pool.
    }

    void DX12Device::destroy_texture(anxiety::rendering::rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_slot(handle));
    }

    // Command buffer -----------------------------------------------------------------------------
    std::unique_ptr<anxiety::rendering::rhi::ICommandBuffer> DX12Device::create_command_buffer() {
        if (!m_valid) return nullptr;

        ComPtr<ID3D12CommandAllocator> allocator;
        HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommandAllocator falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return nullptr;
        }

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateCommandList falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return nullptr;
        }

        // Se cierra inmediatamente — DX12CommandBuffer::begin() lo abre antes de grabar.
        cmdList->Close();

        return std::make_unique<DX12CommandBuffer>(*this, std::move(allocator), std::move(cmdList));
    }

    // Swapchain ----------------------------------------------------------------------------------
    std::unique_ptr<anxiety::rendering::rhi::ISwapchain> DX12Device::create_swapchain(const anxiety::rendering::rhi::SwapchainDesc& desc) {
        if (!m_valid) return nullptr;
        if (!desc.native_window_handle) {
            LOG_ERROR(k_category, "createSwapchain: nativeWindowHandle es nulo.");
            return nullptr;
        }
        return std::make_unique<DX12Swapchain>(*this, desc);
    }

    // Envío y sincronización -------------------------------------------------------------------
    void DX12Device::submit(anxiety::rendering::rhi::ICommandBuffer& cmd) {
        if (!m_valid) return;
        auto& dx12Cmd = static_cast<DX12CommandBuffer&>(cmd);
        ID3D12CommandList* lists[] = { dx12Cmd.native_list() };
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
        assert(m_rtv_next_slot < k_rtv_heap_size && "Heap de RTV agotado");
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(m_rtv_next_slot) * m_rtv_desc_size;
        ++m_rtv_next_slot;
        return h;
    }

    // Registro de texturas externas ------------------------------------------------------------------
    anxiety::rendering::rhi::TextureHandle DX12Device::register_external_texture(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtv, anxiety::rendering::rhi::ResourceState state) {
        const uint32_t slot = allocate_texture_slot();
        auto& s = m_textures[slot];
        s.resource = resource;                     // AddRef del ComPtr
        s.rtv_handle = rtv;
        s.has_RTV = true;
        s.state = state;
        s.alive = true;
        return { static_cast<uint64_t>(slot) + 1u };
    }

    void DX12Device::unregister_texture(anxiety::rendering::rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        free_texture_slot(to_slot(handle));
    }

    // Pool de recursos de textura ------------------------------------------------------------------
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
        m_textures[slot] = TextureSlot{};           // libera el ComPtr, reinicia todos los campos
        m_texture_free_list.push_back(slot);
    }

    // Consultas de recursos ---------------------------------------------------------------------
    ID3D12Resource* DX12Device::lookup_texture(anxiety::rendering::rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return nullptr;
        const uint32_t slot = to_slot(h);
        if (slot >= m_textures.size() || !m_textures[slot].alive) return nullptr;
        return m_textures[slot].resource.Get();
    }

    anxiety::rendering::rhi::ResourceState DX12Device::texture_state(anxiety::rendering::rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return anxiety::rendering::rhi::ResourceState::Undefined;
        const uint32_t slot = to_slot(h);
        if (slot >= m_textures.size()) return anxiety::rendering::rhi::ResourceState::Undefined;
        return m_textures[slot].state;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::texture_RTV(anxiety::rendering::rhi::TextureHandle h) const noexcept {
        if (!h.is_valid()) return { 0 };
        const uint32_t slot = to_slot(h);
        if (slot >= m_textures.size() || !m_textures[slot].has_RTV) return { 0 };
        return m_textures[slot].rtv_handle;
    }

    void DX12Device::set_texture_state(anxiety::rendering::rhi::TextureHandle h, anxiety::rendering::rhi::ResourceState new_state) {
        if (!h.is_valid()) return;
        const uint32_t slot = to_slot(h);
        if (slot < m_textures.size()) m_textures[slot].state = new_state;
    }

} // namespace anxiety::rendering::backend::dx12
