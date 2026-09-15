#pragma once

#include "../../rhi/ICommandBuffer.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace anxiety::rendering::backend::dx12 {
    using Microsoft::WRL::ComPtr;

    class DX12Device;                               // declaración adelantada — el tipo completo solo hace falta en el .cpp

    // DX12CommandBuffer — implementación de rhi::ICommandBuffer para DirectX 12 -------------------
    // Envuelve un par ID3D12CommandAllocator + ID3D12GraphicsCommandList4. El allocator se reinicia
    // dentro de begin() — solo es seguro después de que la GPU haya terminado de usar la grabación
    // anterior (llama a device.wait_idle() entre fotogramas).
    // --------------------------------------------------------------------------------------------
    class DX12CommandBuffer final : public anxiety::rendering::rhi::ICommandBuffer {
    public:
        DX12CommandBuffer(DX12Device& device, ComPtr<ID3D12CommandAllocator> allocator, ComPtr<ID3D12GraphicsCommandList> cmd_list);
        ~DX12CommandBuffer() override = default;

        // rhi::ICommandBuffer --------------------------------------------------------------------
        void begin() override;
        void end()   override;

        void resource_barrier(anxiety::rendering::rhi::TextureHandle texture, anxiety::rendering::rhi::ResourceState before, anxiety::rendering::rhi::ResourceState after) override;
        void clear_render_target(anxiety::rendering::rhi::TextureHandle rt, const anxiety::rendering::rhi::ClearColor& color)                                              override;
        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)                                                          override;
        void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z)                                                                                             override;

        // Accesor interno -------------------------------------------------------------------------
        [[nodiscard]] ID3D12GraphicsCommandList* native_list() const noexcept { return m_cmd_list.Get(); }

    private:
        DX12Device& m_device;
        ComPtr<ID3D12CommandAllocator>       m_allocator;
        ComPtr<ID3D12GraphicsCommandList>    m_cmd_list;
    };
} // namespace anxiety::rendering::backend::dx12
