#pragma once

#include "../../rhi/IDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace anxiety::rendering::backend::dx12 {
    using Microsoft::WRL::ComPtr;

    // TextureSlot — una entrada del pool de texturas del dispositivo ----------------------------
    struct TextureSlot {
        ComPtr<ID3D12Resource>                  resource;
        D3D12_CPU_DESCRIPTOR_HANDLE             rtv_handle = { 0 };
        anxiety::rendering::rhi::ResourceState  state = anxiety::rendering::rhi::ResourceState::Undefined;
        bool                                    has_RTV = false;
        bool                                    alive = false;
    };

    // DX12Device — implementación de rhi::IDevice para DirectX 12 --------------------------------
    // Construcción:
    //   Se construye directamente; llama a is_valid() antes de usarlo. Si is_valid() devuelve
    //   false, ya se ha registrado un error detallado en el log y el dispositivo no debe usarse.
    //
    //   auto dev = std::make_unique<DX12Device>(enable_debug_layer);
    //   if (!dev->is_valid()) { return false; }
    // --------------------------------------------------------------------------------------------
    class DX12Device final : public anxiety::rendering::rhi::IDevice {
    public:
        explicit DX12Device(bool enable_debug_layer = false);
        ~DX12Device() override;

        // Devuelve false si la creación del dispositivo falló (el error ya se ha registrado en el log).
        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "DirectX 12"; }

        [[nodiscard]] anxiety::rendering::rhi::BufferHandle  create_buffer(const anxiety::rendering::rhi::BufferDesc&)   override;
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle create_texture(const anxiety::rendering::rhi::TextureDesc&) override;
        void destroy_buffer(anxiety::rendering::rhi::BufferHandle)   override;
        void destroy_texture(anxiety::rendering::rhi::TextureHandle) override;

        [[nodiscard]] std::unique_ptr<anxiety::rendering::rhi::ICommandBuffer> create_command_buffer()                                         override;
        [[nodiscard]] std::unique_ptr<anxiety::rendering::rhi::ISwapchain>     create_swapchain(const anxiety::rendering::rhi::SwapchainDesc&) override;

        void submit(anxiety::rendering::rhi::ICommandBuffer&) override;
        void wait_idle()                                      override;

        // Accesores internos (usados por DX12Swapchain y DX12CommandBuffer) -----------------------
        [[nodiscard]] ID3D12Device* d3d_device()    const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue* command_queue() const noexcept { return m_cmd_queue.Get(); }
        [[nodiscard]] IDXGIFactory4* dxgi_factory()  const noexcept { return m_factory.Get(); }

        // Reserva un slot del heap de RTV no visible desde el shader. Devuelve un handle de descriptor de CPU para ese slot.
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE allocate_RTV();

        // Registra un ID3D12Resource externo (p. ej. el backbuffer del swapchain) como un TextureHandle. El dispositivo
        // mantiene una referencia ComPtr; quien llama debe invocar unregister_texture() cuando el recurso deje de ser válido.
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle register_external_texture(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtv, anxiety::rendering::rhi::ResourceState state);

        // Libera un slot previamente registrado mediante register_external_texture().
        void unregister_texture(anxiety::rendering::rhi::TextureHandle handle);

        // Consultas de recursos (seguras: devuelven nullptr / el valor por defecto si el handle es inválido).
        [[nodiscard]] ID3D12Resource* lookup_texture(anxiety::rendering::rhi::TextureHandle)                                         const noexcept;
        [[nodiscard]] anxiety::rendering::rhi::ResourceState  texture_state(anxiety::rendering::rhi::TextureHandle)                                         const noexcept;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE             texture_RTV(anxiety::rendering::rhi::TextureHandle)                                         const noexcept;
        void                                                  set_texture_state(anxiety::rendering::rhi::TextureHandle, anxiety::rendering::rhi::ResourceState);

    private:
        [[nodiscard]] uint32_t allocate_texture_slot();
        void                   free_texture_slot(uint32_t slot);

        static constexpr uint32_t k_rtv_heap_size = 64;

        bool m_valid = false;

        ComPtr<IDXGIFactory4>      m_factory;
        ComPtr<IDXGIAdapter1>      m_adapter;
        ComPtr<ID3D12Device>       m_device;
        ComPtr<ID3D12CommandQueue> m_cmd_queue;

        // Fence usada exclusivamente por wait_idle().
        ComPtr<ID3D12Fence>        m_idle_fence;
        uint64_t                   m_idle_fence_value = 0;
        HANDLE                     m_idle_event = nullptr;

        // Heap de RTV no visible desde el shader (solo acceso desde la CPU).
        ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
        uint32_t                     m_rtv_desc_size = 0;
        uint32_t                     m_rtv_next_slot = 0;       // asignador incremental

        // Pool de recursos de textura.
        std::vector<TextureSlot>  m_textures;
        std::vector<uint32_t>     m_texture_free_list;
    };
} // namespace anxiety::rendering::backend::dx12
