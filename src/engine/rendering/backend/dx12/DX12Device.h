#ifdef ANXIETY_BACKEND_DX12

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
        ComPtr<ID3D12Resource>      resource;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle  = { 0 };
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle  = { 0 };
        rhi::ResourceState          state       = rhi::ResourceState::Undefined;
        uint32_t                    width       = 0;
        uint32_t                    height      = 0;
        DXGI_FORMAT                 dxgi_format = DXGI_FORMAT_UNKNOWN;
        bool                        has_RTV     = false;
        bool                        has_DSV     = false;
        bool                        alive       = false;
    };

    // BufferSlot — una entrada del pool de recursos de buffer del dispositivo ----------------------
    struct BufferSlot {
        ComPtr<ID3D12Resource>    resource;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_VA = 0;
        uint64_t                  size_bytes = 0;
        bool                      alive = false;
    };

    // SRVDescriptor — par de handles CPU + GPU del heap SRV visible desde el shader ----------------
    struct SRVDescriptor {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = { 0 };
    };

    // DX12Device — implementación de rhi::IDevice para DirectX 12 --------------------------------
    // Construcción:
    //   Se construye directamente; llama a is_valid() antes de usarlo. Si is_valid() devuelve
    //   false, ya se ha registrado un error detallado en el log y el dispositivo no debe usarse.
    //
    //   auto dev = std::make_unique<DX12Device>(enable_debug_layer);
    //   if (!dev->is_valid()) { return false; }
    // --------------------------------------------------------------------------------------------
    class DX12Device final : public rhi::IDevice {
    public:
        explicit DX12Device(bool enable_debug_layer = false);
        ~DX12Device() override;

        // Devuelve false si la creación del dispositivo falló (el error ya se ha registrado en el log).
        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "DirectX 12"; }

        [[nodiscard]] rhi::BufferHandle  create_buffer(const rhi::BufferDesc& desc, const void* initial_data = nullptr, size_t initial_data_sz = 0)   override;
        [[nodiscard]] rhi::TextureHandle create_texture(const rhi::TextureDesc&)                                                                      override;
        void destroy_buffer(rhi::BufferHandle)   override;
        void destroy_texture(rhi::TextureHandle) override;

        void write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) override;

        void upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) override;

        [[nodiscard]] std::vector<uint8_t>                 compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) override;
        [[nodiscard]] std::unique_ptr<rhi::IShader>        create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage)                              override;
        [[nodiscard]] std::unique_ptr<rhi::IPipeline>      create_pipeline(const rhi::PipelineDesc& desc)                                                  override;
        [[nodiscard]] std::unique_ptr<rhi::IDescriptorSet> create_descriptor_set(const rhi::DescriptorSetLayout& layout)                                   override;

        [[nodiscard]] std::unique_ptr<rhi::ICommandBuffer> create_command_buffer()                     override;
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain>     create_swapchain(const rhi::SwapchainDesc&) override;

        void submit(rhi::ICommandBuffer&) override;
        void wait_idle()                  override;

        // Accesores internos (usados por DX12Swapchain y DX12CommandBuffer) -----------------------
        [[nodiscard]] ID3D12Device*         d3d_device()    const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue*   command_queue() const noexcept { return m_cmd_queue.Get(); }
        [[nodiscard]] IDXGIFactory4*        dxgi_factory()  const noexcept { return m_factory.Get(); }
        [[nodiscard]] ID3D12DescriptorHeap* srv_heap()      const noexcept { return m_srv_heap.Get(); }

        // Reserva un slot del heap de RTV no visible desde el shader. Devuelve un handle de descriptor de CPU para ese slot.
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE allocate_RTV();
        // Libera un slot reservado con allocate_RTV() para que pueda reutilizarse. Debe llamarse
        // siempre que se destruya el recurso dueño de ese RTV (backbuffer o render target) — de lo
        // contrario cada resize()/recreate() agota el heap tras k_rtv_heap_size ciclos.
        void free_RTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

        // Heap de DSV (no visible desde el shader, lado de CPU).
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE allocate_DSV();
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE texture_DSV(rhi::TextureHandle) const noexcept;

        // Heap SRV/CBV/UAV visible desde el shader — usado por DX12DescriptorSet para bindings de textura.
        [[nodiscard]] SRVDescriptor allocate_SRV_descriptor();

        // Registra un ID3D12Resource externo (p. ej. el backbuffer del swapchain) como un TextureHandle. Las
        // dimensiones se leen del propio desc del recurso.
        [[nodiscard]] rhi::TextureHandle register_external_texture(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtv, rhi::ResourceState state);

        // Libera un slot previamente registrado mediante register_external_texture().
        void unregister_texture(rhi::TextureHandle handle);

        // Consultas de texturas (seguras: devuelven nullptr / el valor por defecto si el handle es inválido).
        [[nodiscard]] ID3D12Resource*               lookup_texture(rhi::TextureHandle)                        const noexcept;
        [[nodiscard]] rhi::ResourceState            texture_state(rhi::TextureHandle)                         const noexcept;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE   texture_RTV(rhi::TextureHandle)                           const noexcept;
        [[nodiscard]] std::pair<uint32_t, uint32_t> texture_extent(rhi::TextureHandle)                        const noexcept;
        void                                        set_texture_state(rhi::TextureHandle, rhi::ResourceState);

        // Consultas de buffers (seguras: devuelven nullptr / el valor por defecto si el handle es inválido).
        [[nodiscard]] ID3D12Resource*             lookup_buffer(rhi::BufferHandle)  const noexcept;
        [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS   buffer_GPU_VA(rhi::BufferHandle)  const noexcept;
        [[nodiscard]] uint64_t                    buffer_size(rhi::BufferHandle)    const noexcept;

    private:
        [[nodiscard]] uint32_t allocate_texture_slot();
        void                   free_texture_slot(uint32_t slot);
        [[nodiscard]] uint32_t allocate_buffer_slot();
        void                   free_buffer_slot(uint32_t slot);

        static constexpr uint32_t k_rtv_heap_size = 64;
        static constexpr uint32_t k_dsv_heap_size = 16;
        static constexpr uint32_t k_srv_heap_size = 1024;

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
        std::vector<uint32_t>        m_rtv_free_list;           // slots liberados, reutilizables antes de crecer

        // Heap de DSV no visible desde el shader (acceso solo desde CPU).
        ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
        uint32_t                     m_dsv_desc_size = 0;
        uint32_t                     m_dsv_next_slot = 0;

        // Heap CBV/SRV/UAV visible desde el shader.
        ComPtr<ID3D12DescriptorHeap> m_srv_heap;
        uint32_t                     m_srv_desc_size = 0;
        uint32_t                     m_srv_next_slot = 0;

        // Pool de recursos de textura.
        std::vector<TextureSlot>  m_textures;
        std::vector<uint32_t>     m_texture_free_list;
        std::vector<BufferSlot>   m_buffers;
        std::vector<uint32_t>     m_buffer_free_list;
    };
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12