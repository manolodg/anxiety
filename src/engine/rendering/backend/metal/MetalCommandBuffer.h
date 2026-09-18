// MetalCommandBuffer.h — implementación de ICommandBuffer para Metal.
// void* envuelve todos los objetos ObjC para mantener el header seguro en C++.
#pragma once

#ifdef ANXIETY_BACKEND_METAL

#include "../../rhi/ICommandBuffer.h"
#include "../../rhi/RHITypes.h"
#include "../../rhi/PipelineDesc.h"

namespace anxiety::rendering::backend::metal {
    class MetalDevice;

    class MetalCommandBuffer final : public rhi::ICommandBuffer {
    public:
        explicit MetalCommandBuffer(MetalDevice* device);
        ~MetalCommandBuffer() override;

        // ICommandBuffer -------------------------------------------------------------------------
        void begin() override;
        void end()   override;

        void resource_barrier(rhi::TextureHandle texture, rhi::ResourceState before, rhi::ResourceState after)     override;
        void clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color)                              override;
        void clear_depth_stencil(rhi::TextureHandle depth, float depth_val = 1.0f, uint8_t stencil = 0)            override;

        void bind_pipeline(rhi::IPipeline& pipeline)                                                               override;
        void bind_descriptor_set(uint32_t set, rhi::IDescriptorSet& descriptor_set)                                override;

        void bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset = 0, uint32_t stride = 0) override;
        void bind_index_buffer(rhi::BufferHandle handle, uint64_t offset = 0, bool use_32_bit = true)              override;

        void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0)                                  override;
        void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) override;

        void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z)                                     override;

        // Accesor interno -------------------------------------------------------------------------
        // Devuelve el id<MTLCommandBuffer> subyacente (como void*) para IDevice::submit.
        void* mtl_command_buffer() const noexcept { return m_cmd_buf; }

    private:
        MetalDevice* m_device      = nullptr;
        void*        m_cmd_buf     = nullptr;          // id<MTLCommandBuffer>
        void*        m_render_enc  = nullptr;          // id<MTLRenderCommandEncoder>
        void*        m_compute_enc = nullptr;          // id<MTLComputeCommandEncoder>

        // Estado del render pass acumulado entre clear_render_target y bind_pipeline.
        void*           m_pending_color_tex       = nullptr;  // id<MTLTexture>
        void*           m_pending_depth_tex       = nullptr;  // id<MTLTexture>
        rhi::ClearColor m_pending_clear           = {};
        float           m_pending_depth_clear     = 1.f;
        uint8_t         m_pending_stencil_clear   = 0;
        bool            m_has_pending_clear       = false;
        bool            m_has_pending_depth_clear = false;
        bool            m_in_render_pass          = false;

        // Topología actual del pipeline (necesaria para las llamadas draw).
        rhi::PrimitiveTopology m_topology = rhi::PrimitiveTopology::TriangleList;

        // Estado del buffer de índices.
        void*    m_index_buf      = nullptr;  // id<MTLBuffer>
        uint64_t m_index_offset   = 0;
        bool     m_index_is_32bit = true;

        // Inicia un render pass para el/los render target(s) actualmente pendiente(s).
        // Se llama de forma perezosa desde bind_pipeline si aún no hay un render pass activo.
        void begin_render_pass_if_needed();
        void end_render_pass();
        void end_compute_encoder();
    };
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
