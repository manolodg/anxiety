#include "DX11CommandBuffer.h"
#include "DX11Device.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11CommandBuffer::DX11CommandBuffer(DX11Device& device, ComPtr<ID3D11DeviceContext> deferred) : m_device(device), m_deferred(std::move(deferred)) {}

    // Ciclo de vida ----------------------------------------------------------------------------------
    void DX11CommandBuffer::begin() {
        m_cmd_list.Reset();
        m_deferred->ClearState();
    }

    void DX11CommandBuffer::end() {
        // Produce la lista de comandos; FALSE = NO restaurar el estado del contexto inmediato.
        HRESULT hr = m_deferred->FinishCommandList(FALSE, &m_cmd_list);
        if (FAILED(hr)) LOGF_ERROR(k_category, "FinishCommandList falló: 0x{:08X}", static_cast<uint32_t>(hr));
    }

    // Gestión del render target -------------------------------------------------------------------
    void DX11CommandBuffer::clear_render_target(anxiety::rendering::rhi::TextureHandle handle, const anxiety::rendering::rhi::ClearColor& color) {
        ID3D11RenderTargetView* rtv = m_device.lookup_RTV(handle);
        if (!rtv) {
            LOG_WARNING(k_category, "clearRenderTarget: handle inválido.");
            return;
        }
        float rgba[4] = { color.r, color.g, color.b, color.a };
        m_deferred->ClearRenderTargetView(rtv, rgba);
    }

    // Llamadas de dibujo ---------------------------------------------------------------------------------
    void DX11CommandBuffer::draw(uint32_t /*vertex_count*/, uint32_t /*instance_count*/, uint32_t /*first_vertex*/, uint32_t /*first_instance*/) {
        // Requiere un PSO y vertex buffers — todavía no implementado.
    }

    // Dispatch de cómputo ---------------------------------------------------------------------------
    void DX11CommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        m_deferred->Dispatch(groups_X, groups_Y, groups_Z);
    }
} // namespace anxiety::rendering::backend::dx11
