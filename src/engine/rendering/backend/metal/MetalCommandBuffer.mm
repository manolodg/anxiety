// Objective-C++ — compilado únicamente en macOS/iOS con el framework Metal.
#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>

#include "MetalCommandBuffer.h"
#include "MetalDevice.h"
#include "MetalPipeline.h"
#include "MetalDescriptorSet.h"
#include "MetalHelpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::metal {
    // Construcción / destrucción --------------------------------------------------------------------
    MetalCommandBuffer::MetalCommandBuffer(MetalDevice* device) : m_device(device) {}
    MetalCommandBuffer::~MetalCommandBuffer() {
        end_render_pass();
        end_compute_encoder();
        if (m_cmd_buf) {
            (void)(__bridge_transfer id<MTLCommandBuffer>)m_cmd_buf;
            m_cmd_buf = nullptr;
        }
    }

    // Ciclo de vida --------------------------------------------------------------------------------
    void MetalCommandBuffer::begin() {
        @autoreleasepool {
            // Libera cualquier command buffer retenido previamente.
            end_render_pass();
            end_compute_encoder();
            if (m_cmd_buf) {
                (void)(__bridge_transfer id<MTLCommandBuffer>)m_cmd_buf;
                m_cmd_buf = nullptr;
            }

            id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)m_device->mtl_command_queue();
            id<MTLCommandBuffer> cmd = [queue commandBuffer];
            m_cmd_buf = (__bridge_retained void*)cmd;

            // Reinicia el estado pendiente.
            m_pending_color_tex       = nullptr;
            m_pending_depth_tex       = nullptr;
            m_has_pending_clear       = false;
            m_has_pending_depth_clear = false;
            m_in_render_pass          = false;
            m_index_buf               = nullptr;
            m_index_offset            = 0;
            m_index_is_32bit          = true;
        }
    }

    void MetalCommandBuffer::end() {
        end_render_pass();
        end_compute_encoder();
    }

    // Ayudantes privados -------------------------------------------------------------------------
    void MetalCommandBuffer::begin_render_pass_if_needed() {
        if (m_in_render_pass) return;
        if (!m_pending_color_tex && !m_pending_depth_tex) return;

        @autoreleasepool {
            MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];

            if (m_pending_color_tex) {
                id<MTLTexture> color_tex = (__bridge id<MTLTexture>)m_pending_color_tex;
                auto& att = rpd.colorAttachments[0];
                att.texture     = color_tex;
                att.loadAction  = to_MTL_load_action(m_has_pending_clear);
                att.storeAction = MTLStoreActionStore;
                if (m_has_pending_clear) att.clearColor = MTLClearColorMake(m_pending_clear.r, m_pending_clear.g, m_pending_clear.b, m_pending_clear.a);
            }

            if (m_pending_depth_tex) {
                id<MTLTexture> depth_tex = (__bridge id<MTLTexture>)m_pending_depth_tex;
                rpd.depthAttachment.texture     = depth_tex;
                rpd.depthAttachment.loadAction  = to_MTL_load_action(m_has_pending_depth_clear);
                rpd.depthAttachment.storeAction = MTLStoreActionStore;
                if (m_has_pending_depth_clear) rpd.depthAttachment.clearDepth = m_pending_depth_clear;
            }

            id<MTLCommandBuffer> cmd   = (__bridge id<MTLCommandBuffer>)m_cmd_buf;
            id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:rpd];
            m_render_enc  = (__bridge_retained void*)enc;
            m_in_render_pass = true;
        }
    }

    void MetalCommandBuffer::end_render_pass() {
        if (!m_in_render_pass || !m_render_enc) return;
        @autoreleasepool {
            id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_enc;
            [enc endEncoding];
            (void)(__bridge_transfer id<MTLRenderCommandEncoder>)m_render_enc;
            m_render_enc     = nullptr;
            m_in_render_pass = false;
        }
    }

    void MetalCommandBuffer::end_compute_encoder() {
        if (!m_compute_enc) return;
        @autoreleasepool {
            id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)m_compute_enc;
            [enc endEncoding];
            (void)(__bridge_transfer id<MTLComputeCommandEncoder>)m_compute_enc;
            m_compute_enc = nullptr;
        }
    }

    // Barreras -----------------------------------------------------------------------------------
    void MetalCommandBuffer::resource_barrier(anxiety::rendering::rhi::TextureHandle /*texture*/, anxiety::rendering::rhi::ResourceState /*before*/, anxiety::rendering::rhi::ResourceState /*after*/) {
        // Metal usa un modelo implícito de seguimiento de hazards. Solo hacen falta barriers explícitas para MTLFence / MTLEvent en escenarios avanzados con varias colas; en esta implementación de una sola cola son no-ops.
    }

    // Operaciones sobre el render target -----------------------------------------------------------
    void MetalCommandBuffer::clear_render_target(anxiety::rendering::rhi::TextureHandle rt, const anxiety::rendering::rhi::ClearColor& color) {
        // Termina cualquier render pass activo para poder empezar uno nuevo con un clear.
        end_render_pass();

        if (!rt.is_valid()) return;
        auto& slot          = m_device->tex_slot(rt);
        m_pending_color_tex = slot.texture;
        m_pending_clear     = color;
        m_has_pending_clear = true;
    }

    void MetalCommandBuffer::clear_depth_stencil(anxiety::rendering::rhi::TextureHandle depth, float depth_val, uint8_t stencil) {
        end_render_pass();

        if (!depth.is_valid()) return;
        auto& slot                = m_device->tex_slot(depth);
        m_pending_depth_tex       = slot.texture;
        m_pending_depth_clear     = depth_val;
        m_pending_stencil_clear   = stencil;
        m_has_pending_depth_clear = true;
    }

        // Vinculación del estado del pipeline ----------------------------------------------------------
    void MetalCommandBuffer::bind_pipeline(anxiety::rendering::rhi::IPipeline& pipeline) {
        @autoreleasepool {
            begin_render_pass_if_needed();
            if (!m_render_enc) {
                LOGF_ERROR("Metal", "bind_pipeline: no hay un render pass activo");
                return;
            }

            auto& mp  = static_cast<MetalPipeline&>(pipeline);
            id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_enc;

            [enc setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)mp.mtl_pipeline_state()];

            if (mp.mtl_depth_state()) {
                [enc setDepthStencilState:(__bridge id<MTLDepthStencilState>)mp.mtl_depth_state()];
            }

            [enc setCullMode:to_MTL_cull_mode(mp.cull_mode())];
            [enc setTriangleFillMode:to_MTL_triangle_fill_mode(mp.fill_mode())];
            [enc setFrontFacingWinding:to_MTL_winding(mp.front_face_ccw())];

            m_topology = mp.topology();
        }
    }

    void MetalCommandBuffer::bind_descriptor_set(uint32_t set, anxiety::rendering::rhi::IDescriptorSet& descriptor_set) {
        if (!m_render_enc) return;
        static_cast<MetalDescriptorSet&>(descriptor_set).apply(m_render_enc, set);
    }

    // Buffers de vértices / índices -----------------------------------------------------------------
    void MetalCommandBuffer::bind_vertex_buffer(uint32_t slot, anxiety::rendering::rhi::BufferHandle handle, uint64_t offset, uint32_t /*stride*/) {
        if (!m_render_enc || !handle.is_valid()) return;
        @autoreleasepool {
            auto& bs = m_device->buf_slot(handle);
            id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_enc;
            [enc setVertexBuffer:(__bridge id<MTLBuffer>)bs.buffer
                         offset:(NSUInteger)offset
                        atIndex:(NSUInteger)slot];
        }
    }

    void MetalCommandBuffer::bind_index_buffer(anxiety::rendering::rhi::BufferHandle handle, uint64_t offset, bool use_32_bit) {
        if (!handle.is_valid()) return;
        auto& bs          = m_device->buf_slot(handle);
        m_index_buf       = bs.buffer;
        m_index_offset    = offset;
        m_index_is_32bit  = use_32_bit;
    }


    // Draw / dispatch ------------------------------------------------------------------------------
    void MetalCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        if (!m_render_enc) return;
        @autoreleasepool {
            id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_enc;
            [enc drawPrimitives:to_MTL_primitive_type(m_topology)
                   vertexStart:(NSUInteger)first_vertex
                   vertexCount:(NSUInteger)vertex_count
                 instanceCount:(NSUInteger)instance_count
                  baseInstance:(NSUInteger)first_instance];
        }
    }

    void MetalCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t  /*vertex_offset*/, uint32_t first_instance) {
        if (!m_render_enc || !m_index_buf) return;
        @autoreleasepool {
            id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_enc;

            MTLIndexType idx_type       = m_index_is_32bit ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
            NSUInteger   bytes_per_idx  = m_index_is_32bit ? 4 : 2;
            NSUInteger   idx_buf_offset = (NSUInteger)(m_index_offset + (uint64_t)first_index * bytes_per_idx);

            [enc drawIndexedPrimitives:to_MTL_primitive_type(m_topology)
                            indexCount:(NSUInteger)index_count
                             indexType:idx_type
                           indexBuffer:(__bridge id<MTLBuffer>)m_index_buf
                     indexBufferOffset:idx_buf_offset
                         instanceCount:(NSUInteger)instance_count
                            baseVertex:0
                          baseInstance:(NSUInteger)first_instance];
        }
    }

    void MetalCommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        // Termina el render pass si está activo y luego abre un compute encoder.
        end_render_pass();

        @autoreleasepool {
            if (!m_compute_enc) {
                id<MTLCommandBuffer> cmd = (__bridge id<MTLCommandBuffer>)m_cmd_buf;
                id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
                m_compute_enc = (__bridge_retained void*)enc;
            }

            id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)m_compute_enc;

            // threadgroupsPerGrid = (groupsX, groupsY, groupsZ) threadsPerThreadgroup = (1,1,1) — es responsabilidad del llamador
            // que coincida con el atributo [[threads_per_threadgroup]] del kernel.
            MTLSize tpg = MTLSizeMake(groups_X, groups_Y, groups_Z);
            MTLSize tpt = MTLSizeMake(1, 1, 1);
            [enc dispatchThreadgroups:tpg threadsPerThreadgroup:tpt];
        }
    }
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
