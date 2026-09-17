#ifdef ANXIETY_BACKEND_DX11

#pragma once

#include "../../rhi/ICommandBuffer.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    class DX11Device;    // forward
    class DX11Pipeline;  // forward

    // DX11CommandBuffer — implementación de rhi::ICommandBuffer para DirectX 11 ------------------
    // Usa un deferred context de D3D11 para grabar comandos. En end(), el deferred context produce
    // un ID3D11CommandList que submit() del device ejecuta sobre el immediate context.
    //
    // Esto encaja limpiamente con el modelo de grabación / envío sin ejecutar draw calls en el
    // momento de grabar, preservando las mismas garantías de orden observables que los backends
    // DX12 y Vulkan.
    //
    // El deferred context se conserva y se reutiliza entre frames; begin() llama a ClearState()
    // para reiniciarlo antes de cada sesión de grabación.
    // --------------------------------------------------------------------------------------------
    class DX11CommandBuffer final : public rhi::ICommandBuffer {
    public:
        DX11CommandBuffer(DX11Device& device, ComPtr<ID3D11DeviceContext> deferred);
        ~DX11CommandBuffer() override = default;

        // rhi::ICommandBuffer --------------------------------------------------------------------
        void begin() override;
        void end()   override;

        // Las transiciones en DX11 son implícitas — resourceBarrier no hace nada.
        void resource_barrier(rhi::TextureHandle, rhi::ResourceState, rhi::ResourceState)                         override {}
        void clear_render_target(rhi::TextureHandle, const rhi::ClearColor&)                                      override;
        void clear_depth_stencil(rhi::TextureHandle, float, uint8_t)                                              override;
        
        void bind_pipeline(rhi::IPipeline& pipeline)                                                              override;
        void bind_descriptor_set(uint32_t set, rhi::IDescriptorSet& ds)                                           override;
        void bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset, uint32_t stride)        override;
        void bind_index_buffer(rhi::BufferHandle handle, uint64_t offset, bool use_32_bit)                        override;

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)                              override;
        void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override;
        
        void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z)                                    override;

        // Accesor interno (usado por DX11Device::submit) -----------------------------------------
        [[nodiscard]] ID3D11CommandList* command_list() const noexcept { return m_cmd_list.Get(); }

    private:
        // Vuelve a aplicar el par RTV / DSV actual tras cambios de estado.
        void flush_render_targets();

        DX11Device&                 m_device;
        ComPtr<ID3D11DeviceContext> m_deferred;
        ComPtr<ID3D11CommandList>   m_cmd_list;

        DX11Pipeline*           m_current_pipeline = nullptr;
        ID3D11RenderTargetView* m_active_rtv       = nullptr;
        ID3D11DepthStencilView* m_active_dsv       = nullptr;
        uint32_t                m_rt_width         = 0;
        uint32_t                m_rt_height        = 0;
    };
} // namespace anxiety::rendering::backend::dx11

#endif ANXIETY_BACKEND_DX11