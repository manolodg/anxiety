#ifdef ANXIETY_BACKEND_DX11

#include "DX11CommandBuffer.h"
#include "DX11Device.h"
#include "DX11Pipeline.h"
#include "DX11DescriptorSet.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11CommandBuffer::DX11CommandBuffer(DX11Device& device, ComPtr<ID3D11DeviceContext> deferred) : m_device(device), m_deferred(std::move(deferred)) {}

    // Ciclo de vida ----------------------------------------------------------------------------------
    void DX11CommandBuffer::begin() {
        m_cmd_list.Reset();
        m_current_pipeline = nullptr;
        m_active_rtv       = nullptr;
        m_active_dsv       = nullptr;
        m_rt_width         = 0;
        m_rt_height        = 0;
        m_deferred->ClearState();
    }

    void DX11CommandBuffer::end() {
        // Produce la lista de comandos; FALSE = NO restaurar el estado del contexto inmediato.
        HRESULT hr = m_deferred->FinishCommandList(FALSE, &m_cmd_list);
        if (FAILED(hr)) LOGF_ERROR(k_category, "FinishCommandList falló: 0x{:08X}", static_cast<uint32_t>(hr));
    }

    // Gestión del render target -------------------------------------------------------------------
    void DX11CommandBuffer::clear_render_target(rhi::TextureHandle handle, const rhi::ClearColor& color) {
        ID3D11RenderTargetView* rtv = m_device.lookup_RTV(handle);
        if (!rtv) {
            LOG_WARNING(k_category, "clear_render_target: handle inválido.");
            return;
        }
        float rgba[4] = { color.r, color.g, color.b, color.a };
        m_deferred->ClearRenderTargetView(rtv, rgba);

        m_active_rtv = rtv;
        auto [w, h]  = m_device.texture_extent(handle);
        m_rt_width   = w;
        m_rt_height  = h;
        flush_render_targets();
    }

    void DX11CommandBuffer::clear_depth_stencil(rhi::TextureHandle handle,
        float depth_val, uint8_t stencil) {
        ID3D11DepthStencilView* dsv = m_device.lookup_DSV(handle);
        if (!dsv) {
            LOG_WARNING(k_category, "clear_depth_stencil: handle inválido.");
            return;
        }
        m_deferred->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth_val, stencil);

        m_active_dsv = dsv;
        flush_render_targets();
    }

    void DX11CommandBuffer::flush_render_targets() {
        m_deferred->OMSetRenderTargets(m_active_rtv ? 1u : 0u, m_active_rtv ? &m_active_rtv : nullptr, m_active_dsv);

        if (m_rt_width > 0 && m_rt_height > 0) {
            D3D11_VIEWPORT vp{};
            vp.Width    = static_cast<float>(m_rt_width);
            vp.Height   = static_cast<float>(m_rt_height);
            vp.MinDepth = 0.f;
            vp.MaxDepth = 1.f;
            m_deferred->RSSetViewports(1, &vp);
        }
    }

    // Binding de pipeline --------------------------------------------------------------------------
    void DX11CommandBuffer::bind_pipeline(rhi::IPipeline& pipeline) {
        auto& p             = static_cast<DX11Pipeline&>(pipeline);
        m_current_pipeline = &p;

        m_deferred->VSSetShader(p.vs(), nullptr, 0);
        m_deferred->PSSetShader(p.ps(), nullptr, 0);
        m_deferred->IASetInputLayout(p.input_layout());
        m_deferred->IASetPrimitiveTopology(p.topology());
        m_deferred->RSSetState(p.rasterizer_state());
        m_deferred->OMSetDepthStencilState(p.depth_state(), 0);

        static const float k_blend_factor[4] = { 1.f, 1.f, 1.f, 1.f };
        m_deferred->OMSetBlendState(p.blend_state(), k_blend_factor, 0xFFFFFFFFu);

        // Enlaza los samplers estáticos en sus registros de shader declarados.
        const auto& samplers = p.sampler_states();
        for (uint32_t i = 0; i < static_cast<uint32_t>(samplers.size()); ++i) {
            ID3D11SamplerState* ss = samplers[i].Get();
            m_deferred->PSSetSamplers(i, 1, &ss);
            m_deferred->VSSetSamplers(i, 1, &ss);
        }
    }

    void DX11CommandBuffer::bind_descriptor_set(uint32_t /*set*/, rhi::IDescriptorSet& ds) {
        static_cast<DX11DescriptorSet&>(ds).apply(m_deferred.Get());
    }

    // Ensamblado de entrada -------------------------------------------------------------------------
    void DX11CommandBuffer::bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset, uint32_t stride) {
        ID3D11Buffer* buf = m_device.lookup_buffer(handle);
        UINT          off = static_cast<UINT>(offset);
        UINT          str = stride;
        m_deferred->IASetVertexBuffers(slot, 1, &buf, &str, &off);
    }

    void DX11CommandBuffer::bind_index_buffer(rhi::BufferHandle handle, uint64_t offset, bool use_32_bit) {
        ID3D11Buffer* buf = m_device.lookup_buffer(handle);
        DXGI_FORMAT   fmt = use_32_bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        m_deferred->IASetIndexBuffer(buf, fmt, static_cast<UINT>(offset));
    }

    // Llamadas de dibujo ----------------------------------------------------------------------------
    void DX11CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        if (instance_count <= 1 && first_instance == 0) {
            m_deferred->Draw(vertex_count, first_vertex);
        } else {
            m_deferred->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
        }
    }

    void DX11CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
        if (instance_count <= 1 && first_instance == 0) {
            m_deferred->DrawIndexed(index_count, first_index, vertex_offset);
        } else {
            m_deferred->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
        }
    }

    // Dispatch de cómputo ---------------------------------------------------------------------------
    void DX11CommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        m_deferred->Dispatch(groups_X, groups_Y, groups_Z);
    }
} // namespace anxiety::rendering::backend::dx11

#endif ANXIETY_BACKEND_DX11