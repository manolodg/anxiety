#include "DX12CommandBuffer.h"
#include "DX12Device.h"
#include "DX12Helpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx12 {
    DX12CommandBuffer::DX12CommandBuffer(DX12Device& device, ComPtr<ID3D12CommandAllocator> allocator, ComPtr<ID3D12GraphicsCommandList> cmd_list) : m_device(device), m_allocator(std::move(allocator)), m_cmd_list(std::move(cmd_list)) {}

    // Ciclo de vida --------------------------------------------------------------------------------
    void DX12CommandBuffer::begin() {
        // Solo es seguro reiniciar el allocator después de que la GPU haya terminado — quien llama
        // debe garantizar que se llamó a device.waitIdle() antes de reutilizar este command buffer.
        m_allocator->Reset();
        m_cmd_list->Reset(m_allocator.Get(), nullptr);
    }

    void DX12CommandBuffer::end() { m_cmd_list->Close(); }

    // Barreras de recursos ---------------------------------------------------------------------------
    void DX12CommandBuffer::resource_barrier(anxiety::rendering::rhi::TextureHandle texture, anxiety::rendering::rhi::ResourceState before, anxiety::rendering::rhi::ResourceState after) {
        ID3D12Resource* resource = m_device.lookup_texture(texture);
        if (!resource) {
            LOG_WARNING("RHI", "resourceBarrier: handle de textura inválido.");
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = to_D3D12_state(before);
        barrier.Transition.StateAfter = to_D3D12_state(after);

        m_cmd_list->ResourceBarrier(1, &barrier);
        m_device.set_texture_state(texture, after);
    }

    // Operaciones sobre el render target ---------------------------------------------------------
    void DX12CommandBuffer::clear_render_target(anxiety::rendering::rhi::TextureHandle rt, const anxiety::rendering::rhi::ClearColor& color) {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_device.texture_RTV(rt);
        if (!rtv.ptr) {
            LOG_WARNING("RHI", "clearRenderTarget: la textura no tiene RTV.");
            return;
        }

        const FLOAT rgba[4] = { color.r, color.g, color.b, color.a };
        m_cmd_list->ClearRenderTargetView(rtv, rgba, 0, nullptr);
    }

    // Provisionales de draw / dispatch -------------------------------------------------------------
    void DX12CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        // Requiere un PSO y vertex buffers — todavía no implementado.
        m_cmd_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
    }

    void DX12CommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        // Requiere un PSO de cómputo — todavía no implementado.
        m_cmd_list->Dispatch(groups_X, groups_Y, groups_Z);
    }
} // namespace anxiety::rendering::backend::dx12