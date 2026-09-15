#pragma once

#include "../../rhi/ICommandBuffer.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    class DX11Device;    // forward

    // DX11CommandBuffer — implementación de rhi::ICommandBuffer para DirectX 11 -----------------------
    // Usa un contexto diferido (deferred context) de D3D11 para grabar comandos. En end(), el contexto
    // diferido produce una ID3D11CommandList que el submit() del dispositivo ejecuta en el contexto inmediato.
    //
    // Esto encaja perfectamente con el modelo de grabación / envío sin ejecutar draw calls en el momento
    // de la grabación, preservando las mismas garantías de orden observable que los backends DX12 y Vulkan.
    //
    // El contexto diferido se conserva y se reutiliza entre fotogramas; begin() llama a clear_state() para
    // reiniciarlo antes de cada sesión de grabación.
    // --------------------------------------------------------------------------------------------
    class DX11CommandBuffer final : public anxiety::rendering::rhi::ICommandBuffer {
    public:
        DX11CommandBuffer(DX11Device& device, ComPtr<ID3D11DeviceContext> deferred);
        ~DX11CommandBuffer() override = default;

        // rhi::ICommandBuffer --------------------------------------------------------------------
        void begin() override;
        void end()   override;

        // Las transiciones en DX11 son implícitas — resourceBarrier no hace nada.
        void resource_barrier(anxiety::rendering::rhi::TextureHandle, anxiety::rendering::rhi::ResourceState, anxiety::rendering::rhi::ResourceState) override {}
        void clear_render_target(anxiety::rendering::rhi::TextureHandle, const anxiety::rendering::rhi::ClearColor&)                                  override;
        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)                                     override;
        void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z)                                                                        override;

        // Accesor interno (usado por DX11Device::submit) -----------------------------------------
        [[nodiscard]] ID3D11CommandList* command_list() const noexcept { return m_cmd_list.Get(); }

    private:
        DX11Device& m_device;
        ComPtr<ID3D11DeviceContext> m_deferred;
        ComPtr<ID3D11CommandList>   m_cmd_list;
    };
} // namespace anxiety::rendering::backend::dx11
