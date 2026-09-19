#ifdef ANXIETY_BACKEND_OPENGL

#include "GLCommandBuffer.h"
#include "GLDevice.h"
#include "GLPipeline.h"
#include "GLDescriptorSet.h"
#include "Logger.h"

#include <cassert>
#include <cstdint>

namespace anxiety::rendering::backend::opengl {
    // Ciclo de vida de grabación --------------------------------------------------------------------
    void GLCommandBuffer::begin() {
        assert(!m_recording && "GLCommandBuffer::begin() llamado mientras ya se estaba grabando.");
        m_commands.clear();
        m_active_pipeline     = nullptr;
        m_active_vao          = 0;
        m_index_type          = GL_UNSIGNED_INT;
        m_index_buffer_offset = 0;
        m_topology            = GL_TRIANGLES;
        m_recording           = true;
    }

    void GLCommandBuffer::end() {
        assert(m_recording && "GLCommandBuffer::end() llamado sin su begin() correspondiente.");
        m_recording = false;
    }

    // resource_barrier -------------------------------------------------------------------------------
    // GL no tiene una máquina de estados de recursos explícita. Se emite un glMemoryBarrier
    // conservador para que las lecturas posteriores vean cualquier escritura previa. glMemoryBarrier
    // requiere GL 4.2 / GLES 3.1 — no está garantizado en GLES 3.0 ni en el GL 4.1 tope de macOS.
    // Se protege la llamada.
    void GLCommandBuffer::resource_barrier(rhi::TextureHandle /*texture*/, rhi::ResourceState /*before*/, rhi::ResourceState /*after*/) {
        m_commands.push_back([]() {
            if (glMemoryBarrier) {
                glMemoryBarrier(GL_ALL_BARRIER_BITS);
            } else {
                glFlush();
            }
        });
    }

    // clear_render_target ------------------------------------------------------------------------
    void GLCommandBuffer::clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color) {
        const float r = color.r, g = color.g, b = color.b, a = color.a;

        if (rt.id == 1) {
            // Handle centinela == framebuffer por defecto (FBO 0)
            m_commands.push_back([r, g, b, a]() {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glClearColor(r, g, b, a);
                glClear(GL_COLOR_BUFFER_BIT);
            });
        } else {
            push_clear_FBO(rt, color);
        }
    }

    void GLCommandBuffer::push_clear_FBO(rhi::TextureHandle rt, const rhi::ClearColor& color) {
        GLDevice* dev        = m_device;
        const auto rt_handle = rt;
        const float r = color.r, g = color.g, b = color.b, a = color.a;

        m_commands.push_back([dev, rt_handle, r, g, b, a]() {
            const GLTextureSlot& slot = dev->tex_slot(rt_handle);

            // FBO basado en bind (sin DSA) — compatible con macOS 4.1 y GLES.
            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            if (slot.is_depth) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, slot.name, 0);
                glDepthMask(GL_TRUE);
                glClearDepthf(1.0f);
                glClear(GL_DEPTH_BUFFER_BIT);
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.name, 0);
                const GLenum draw_buf = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &draw_buf);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glClearColor(r, g, b, a);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
        });
    }

    // clear_depth_stencil ------------------------------------------------------------------------
    void GLCommandBuffer::clear_depth_stencil(rhi::TextureHandle depth, float depth_val, uint8_t stencil) {
        GLDevice*  dev       = m_device;
        const auto dt_handle = depth;
        const float d        = depth_val;
        const GLint s        = static_cast<GLint>(stencil);

        if (dt_handle.id == 1) {
            // Depth/stencil del framebuffer por defecto
            m_commands.push_back([d, s]() {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDepthMask(GL_TRUE);
                glClearDepthf(d);
                glClearStencil(s);
                glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            });
        } else {
            m_commands.push_back([dev, dt_handle, d, s]() {
                const GLTextureSlot& slot = dev->tex_slot(dt_handle);

                GLuint fbo = 0;
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                // GL_DEPTH24_STENCIL8 es un formato combinado — usar DEPTH_STENCIL_ATTACHMENT.
                // GL_DEPTH_COMPONENT32F no tiene plano de stencil — usar solo DEPTH_ATTACHMENT.
                const bool has_stencil = (slot.internal_format == GL_DEPTH24_STENCIL8);
                const GLenum attach = has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
                glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, slot.name, 0);
                glDepthMask(GL_TRUE);
                glClearDepthf(d);
                glClearStencil(s);
                const GLbitfield clear_bits = GL_DEPTH_BUFFER_BIT | (has_stencil ? GL_STENCIL_BUFFER_BIT : 0u);
                glClear(clear_bits);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &fbo);
            });
        }
    }

    // bind_pipeline ------------------------------------------------------------------------------
    void GLCommandBuffer::bind_pipeline(rhi::IPipeline& pipeline) {
        GLPipeline& gl_pipeline = static_cast<GLPipeline&>(pipeline);

        // Se cachea en tiempo de grabación para usarlo en las siguientes llamadas a
        // bind_vertex_buffer / draw
        m_active_pipeline = &gl_pipeline;
        m_active_vao      = gl_pipeline.vao();
        m_topology        = gl_pipeline.topology();

        // Se captura todo el estado por valor para que la lambda sea autocontenida
        const GLuint program      = gl_pipeline.program();
        const GLuint vao          = gl_pipeline.vao();
        const GLenum topology     = gl_pipeline.topology();   // no se usa en la lambda, se mantiene por claridad
        const bool   cull_enabled = gl_pipeline.cull_enabled();
        const GLenum cull_face    = gl_pipeline.cull_face();
        const bool   front_CCW    = gl_pipeline.front_face_CCW();
        const GLenum fill_mode    = gl_pipeline.fill_mode();
        const bool   depth_test   = gl_pipeline.depth_test();
        const bool   depth_write  = gl_pipeline.depth_write();
        const GLenum depth_func   = gl_pipeline.depth_func();
        const bool   blend_en     = gl_pipeline.blend_enabled();
        const GLenum src_RGB      = gl_pipeline.src_RGB();
        const GLenum dst_RGB      = gl_pipeline.dst_RGB();
        const GLenum eq_RGB       = gl_pipeline.blend_eq_RGB();
        const GLenum src_A        = gl_pipeline.src_A();
        const GLenum dst_A        = gl_pipeline.dst_A();
        const GLenum eq_A         = gl_pipeline.blend_eq_A();

        (void)topology; // topology se usa en draw/draw_indexed, no en esta lambda

        m_commands.push_back([program, vao,
                               cull_enabled, cull_face, front_CCW, fill_mode,
                               depth_test, depth_write, depth_func,
                               blend_en, src_RGB, dst_RGB, eq_RGB, src_A, dst_A, eq_A]() {
            glUseProgram(program);
            glBindVertexArray(vao);

            // Rasterizador
            if (cull_enabled) { 
                glEnable(GL_CULL_FACE); glCullFace(cull_face); 
            } else {
                glDisable(GL_CULL_FACE); 
            }
            glFrontFace(front_CCW ? GL_CCW : GL_CW);
            // glPolygonMode solo existe en GL de escritorio — GLES 3.x no lo expone.
            if (glPolygonMode) glPolygonMode(GL_FRONT_AND_BACK, fill_mode);

            // Depth
            if (depth_test) { 
                glEnable(GL_DEPTH_TEST); glDepthFunc(depth_func); 
            } else {
                glDisable(GL_DEPTH_TEST); 
            }
            glDepthMask(depth_write ? GL_TRUE : GL_FALSE);

            // Blend
            if (blend_en) {
                glEnable(GL_BLEND);
                glBlendFuncSeparate(src_RGB, dst_RGB, src_A, dst_A);
                glBlendEquationSeparate(eq_RGB, eq_A);
            } else {
                glDisable(GL_BLEND);
            }
        });
    }

    // bind_descriptor_set ------------------------------------------------------------------------
    void GLCommandBuffer::bind_descriptor_set(uint32_t /*set*/, rhi::IDescriptorSet& descriptor_set) {
        GLDescriptorSet* ds = static_cast<GLDescriptorSet*>(&descriptor_set);
        m_commands.push_back([ds]() { ds->apply_to_context(); });
    }

    // bind_vertex_buffer -------------------------------------------------------------------------
    // Los punteros de atributos de vértice se configuran en el VAO actualmente vinculado en tiempo
    // de ejecución. Es el equivalente sin DSA de glVertexArrayVertexBuffer + glVertexArrayAttribFormat,
    // y es compatible con GL 4.1 (macOS) y GLES 3.0.
    void GLCommandBuffer::bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset, uint32_t stride) {
        GLDevice*   dev      = m_device;
        GLPipeline* pipeline = m_active_pipeline;
        const auto  h        = handle;
        const auto  s        = slot;
        const GLintptr  off  = static_cast<GLintptr>(offset);
        // Si quien llama pasa stride == 0, se recurre al stride del pipeline.
        const GLsizei   str  = static_cast<GLsizei>(stride > 0 ? stride : (pipeline ? pipeline->vertex_stride() : 0));

        m_commands.push_back([dev, pipeline, s, h, off, str]() {
            const GLBufferSlot& bslot = dev->buf_slot(h);
            glBindBuffer(GL_ARRAY_BUFFER, bslot.name);

            if (pipeline) {
                // La location del atributo es su posición en el layout (igual que en Vulkan): shaderc
                // numera las entradas del VS en orden de declaración. semantic_index NO sirve — POSITION0
                // y COLOR0 valen ambos 0 y pisarían el mismo atributo.
                GLuint next_location = 0;
                for (const auto& attr : pipeline->vertex_attribs()) {
                    const GLuint location = next_location++;
                    if (attr.input_slot != s) continue;

                    const GLVertexTypeInfo ti          = to_GL_vertex_type(attr.format);
                    const GLuint           attrib_idx  = location;
                    // Puntero = inicio del buffer + offset en bytes del atributo por vértice.
                    const void* ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(off) + attr.byte_offset);

                    glEnableVertexAttribArray(attrib_idx);

                    if (ti.type == GL_UNSIGNED_INT && ti.normalized == GL_FALSE) {
                        // Atributo entero — se usa la variante I para evitar la conversión implícita a float.
                        glVertexAttribIPointer(attrib_idx, ti.components, ti.type, str, ptr);
                    } else {
                        glVertexAttribPointer(attrib_idx, ti.components, ti.type, ti.normalized, str, ptr);
                    }
                }
            }
        });
    }

    // bind_index_buffer --------------------------------------------------------------------------
    void GLCommandBuffer::bind_index_buffer(rhi::BufferHandle handle, uint64_t offset, bool use_32_bit) {
        GLDevice*   dev      = m_device;
        const auto  h        = handle;
        const GLenum idx_type = use_32_bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

        // Se actualiza el estado en tiempo de grabación para que las siguientes lambdas de
        // draw_indexed capturen los valores correctos.
        m_index_type          = idx_type;
        m_index_buffer_offset = offset;

        m_commands.push_back([dev, h]() {
            const GLBufferSlot& bslot = dev->buf_slot(h);
            // Vincular GL_ELEMENT_ARRAY_BUFFER modifica el element buffer del VAO actualmente vinculado.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bslot.name);
        });
    }

    // draw -------------------------------------------------------------------------------------------
    void GLCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t /*first_instance*/) {
        // glDrawArraysInstancedBaseInstance (GL 4.2) no existe en macOS 4.1 ni en GLES — se usa
        // glDrawArraysInstanced en su lugar (GL 3.1 / GLES 3.0). Por eso first_instance se ignora.
        const GLenum topo = m_topology;
        m_commands.push_back([topo, vertex_count, instance_count, first_vertex]() {
            glDrawArraysInstanced(topo,
                                  static_cast<GLint>(first_vertex),
                                  static_cast<GLsizei>(vertex_count),
                                  static_cast<GLsizei>(instance_count));
        });
    }

    // draw_indexed -------------------------------------------------------------------------------
    void GLCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t /*first_instance*/) {
        // Se calcula el offset en bytes dentro del index buffer en tiempo de grabación.
        const GLenum  topo     = m_topology;
        const GLenum  idx_type = m_index_type;
        const size_t  idx_size = (idx_type == GL_UNSIGNED_INT) ? 4u : 2u;
        const size_t  idx_byte_offset = m_index_buffer_offset + static_cast<size_t>(first_index) * idx_size;
        const int32_t vo       = vertex_offset;

        // first_instance se ignora por el mismo motivo que en draw() más arriba.
        m_commands.push_back([topo, idx_type, index_count, instance_count, idx_byte_offset, vo, idx_size]() {
            const void* idx_ptr = reinterpret_cast<const void*>(idx_byte_offset);
            if (glDrawElementsInstancedBaseVertex) {
                // GL 3.2 / GLES 3.2 — ruta preferida (soporta vertex_offset).
                glDrawElementsInstancedBaseVertex(topo,
                                                  static_cast<GLsizei>(index_count),
                                                  idx_type,
                                                  idx_ptr,
                                                  static_cast<GLsizei>(instance_count),
                                                  vo);
            } else {
                // Ruta alternativa para GLES 3.0 — vertex_offset se ignora silenciosamente.
                glDrawElementsInstanced(topo,
                                        static_cast<GLsizei>(index_count),
                                        idx_type,
                                        idx_ptr,
                                        static_cast<GLsizei>(instance_count));
            }
        });
    }

    // dispatch ---------------------------------------------------------------------------------------
    // Los compute shaders requieren GL 4.3 / GLES 3.1 — la implementación de OpenGL de macOS tiene un
    // tope de 4.1 y nunca ganó soporte de compute, por lo que ahí glDispatchCompute es nulo. Se protege
    // la llamada en vez de fallar.
    void GLCommandBuffer::dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) {
        m_commands.push_back([groups_X, groups_Y, groups_Z]() {
            if (!glDispatchCompute) {
                LOG_ERROR("GLCommandBuffer", "dispatch() requiere soporte de compute shaders (GL 4.3+ / GLES 3.1+) — no disponible en este driver.");
                return;
            }
            glDispatchCompute(groups_X, groups_Y, groups_Z);
            if (glMemoryBarrier) glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        });
    }

    // execute_all — llamado por GLDevice::submit() -------------------------------------------------
    void GLCommandBuffer::execute_all() {
        for (auto& cmd : m_commands) cmd();
        m_commands.clear();
    }
} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
