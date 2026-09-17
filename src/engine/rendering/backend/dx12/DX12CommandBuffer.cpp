#if ANXIETY_BACKEND_DX12

#include "DX12CommandBuffer.h"
#include "DX12Device.h"
#include "DX12Pipeline.h"
#include "DX12DescriptorSet.h"
#include "DX12Helpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx12 {
    DX12CommandBuffer::DX12CommandBuffer(DX12Device& device, ComPtr<ID3D12CommandAllocator> allocator, ComPtr<ID3D12GraphicsCommandList> cmd_list) : m_device(device), m_allocator(std::move(allocator)), m_cmd_list(std::move(cmd_list)) {}

    // Ciclo de vida --------------------------------------------------------------------------------
    void DX12CommandBuffer::begin() {
        m_allocator->Reset();
        m_cmd_list->Reset(m_allocator.Get(), nullptr);
        m_current_pipeline  = nullptr;
        m_pending_depth_tex = {};

        // Vincula el heap SRV visible desde el shader para que las descriptor tables funcionen.
        auto* heap = m_device.srv_heap();
        if (heap) {
            ID3D12DescriptorHeap* heaps[] = { heap };
            m_cmd_list->SetDescriptorHeaps(1, heaps);
        }
    }

    void DX12CommandBuffer::end() { m_cmd_list->Close(); }

    // Barreras de recursos ---------------------------------------------------------------------------
    void DX12CommandBuffer::resource_barrier(rhi::TextureHandle texture, rhi::ResourceState before, rhi::ResourceState after) {
        ID3D12Resource* resource = m_device.lookup_texture(texture);
        if (!resource) {
            LOG_WARNING("RHI", "resourceBarrier: handle de textura inválido.");
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = to_D3D12_state(before);
        barrier.Transition.StateAfter  = to_D3D12_state(after);

        m_cmd_list->ResourceBarrier(1, &barrier);
        m_device.set_texture_state(texture, after);
    }

    // Operaciones sobre el render target / depth-stencil -----------------------------------------
    void DX12CommandBuffer::clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color) {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_device.texture_RTV(rt);
        if (!rtv.ptr) {
            LOG_WARNING("RHI", "clearRenderTarget: la textura no tiene RTV.");
            return;
        }

        // Vincula el RT (+ el DSV pendiente si se estableció uno este fotograma) y configura el viewport/scissor.
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_pending_depth_tex.is_valid() ? m_device.texture_DSV(m_pending_depth_tex) : D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
        m_cmd_list->OMSetRenderTargets(1, &rtv, FALSE, dsv.ptr ? &dsv : nullptr);

        auto [w, h] = m_device.texture_extent(rt);
        if (w > 0 && h > 0) {
            D3D12_VIEWPORT vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
            D3D12_RECT     sr{    0,    0, static_cast<LONG>(w),  static_cast<LONG>(h) };
            m_cmd_list->RSSetViewports(1, &vp);
            m_cmd_list->RSSetScissorRects(1, &sr);
        }

        const FLOAT rgba[4] = { color.r, color.g, color.b, color.a };
        m_cmd_list->ClearRenderTargetView(rtv, rgba, 0, nullptr);
    }

    void DX12CommandBuffer::clear_depth_stencil(rhi::TextureHandle depth, float depth_val, uint8_t stencil) {
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_device.texture_DSV(depth);
        if (!dsv.ptr) {
            LOG_WARNING("RHI", "clear_depth_stencil: la textura no tiene DSV.");
            return;
        }
        m_pending_depth_tex = depth;
        m_cmd_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth_val, stencil, 0, nullptr);
    }

    // Pipeline state -----------------------------------------------------------------------------
    void DX12CommandBuffer::bind_pipeline(rhi::IPipeline& pipeline) {
        auto& p = static_cast<DX12Pipeline&>(pipeline);
        m_cmd_list->SetGraphicsRootSignature(p.root_signature());
        m_cmd_list->SetPipelineState(p.pso());
        m_cmd_list->IASetPrimitiveTopology(p.d3d_topology());
        m_current_pipeline = &p;
    }

    void DX12CommandBuffer::bind_descriptor_set(uint32_t /*set*/, rhi::IDescriptorSet& ds) {
        static_cast<DX12DescriptorSet&>(ds).bind(m_cmd_list.Get());
    }

    // Vertex / index buffers ---------------------------------------------------------------------
    void DX12CommandBuffer::bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset, uint32_t stride) {
        const D3D12_GPU_VIRTUAL_ADDRESS va   = m_device.buffer_GPU_VA(handle);
        const uint64_t                  size = m_device.buffer_size(handle);
        if (va == 0 || size == 0) {
            LOG_WARNING("RHI", "bindVertexBuffer: handle de buffer inválido.");
            return;
        }
        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = va + offset;
        view.SizeInBytes    = static_cast<UINT>(size - offset);
        view.StrideInBytes  = stride;
        m_cmd_list->IASetVertexBuffers(slot, 1, &view);
    }

    void DX12CommandBuffer::bind_index_buffer(rhi::BufferHandle handle, uint64_t offset, bool use_32_bit) {
        const D3D12_GPU_VIRTUAL_ADDRESS va   = m_device.buffer_GPU_VA(handle);
        const uint64_t                  size = m_device.buffer_size(handle);
        if (va == 0) {
            LOG_WARNING("RHI", "bindIndexBuffer: handle de buffer inválido.");
            return;
        }
        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = va + offset;
        view.SizeInBytes    = static_cast<UINT>(size - offset);
        view.Format         = use_32_bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        m_cmd_list->IASetIndexBuffer(&view);
    }

    // Provisionales de draw / dispatch -------------------------------------------------------------
    void DX12CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        // Requiere un PSO y vertex buffers — todavía no implementado.
        m_cmd_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
    }

    void DX12CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
        m_cmd_list->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void DX12CommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        // Requiere un PSO de cómputo — todavía no implementado.
        m_cmd_list->Dispatch(groups_X, groups_Y, groups_Z);
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12