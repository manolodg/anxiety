#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

#include "GLHelpers.h"
#include "../../rhi/IPipeline.h"
#include "../../rhi/IDescriptorSet.h"
#include "../../rhi/VertexLayout.h"

#include <string>
#include <vector>

namespace anxiety::rendering::backend::opengl {

    // GLPipeline — PSO inmutable para el backend de OpenGL.
    //
    // Posee:
    //   - Un objeto de programa de GL enlazado (shaders de vértice + fragmento)
    //   - Un VAO                        (vinculado en tiempo de draw; los punteros de
    //                                    atributos de vértice se configuran en bind_vertex_buffer)
    //   - Estado de rasterizador / depth / blend del lado de la CPU, reproducido en bind_pipeline
    //
    // Nota: el formato de los atributos de vértice se almacena en m_vertex_attribs y se aplica en
    // GLCommandBuffer::bind_vertex_buffer() usando glVertexAttribPointer, por lo que el VAO en sí
    // no queda totalmente configurado hasta el primer draw posterior a bind_vertex_buffer.
    // Esto evita el DSA (GL 4.5) y es compatible con el tope de GL 4.1 de macOS y con GLES.
    class GLPipeline final : public rhi::IPipeline {
    public:
        GLPipeline() = default;
        ~GLPipeline() override;

        GLPipeline(const GLPipeline&)            = delete;
        GLPipeline& operator=(const GLPipeline&) = delete;

        // IPipeline
        [[nodiscard]] std::string_view debug_name() const noexcept override { return m_name; }

        // Accesores específicos de GL
        [[nodiscard]] GLuint program()         const noexcept { return m_program; }
        [[nodiscard]] GLuint vao()             const noexcept { return m_vao; }
        [[nodiscard]] GLenum topology()        const noexcept { return m_topology; }

        // Estado del rasterizador
        [[nodiscard]] bool   cull_enabled()    const noexcept { return m_cull_enabled;   }
        [[nodiscard]] GLenum cull_face()       const noexcept { return m_cull_face;      }
        [[nodiscard]] bool   front_face_CCW()  const noexcept { return m_front_face_CCW; }
        [[nodiscard]] GLenum fill_mode()       const noexcept { return m_fill_mode;      }

        // Estado de depth
        [[nodiscard]] bool   depth_test()      const noexcept { return m_depth_test;  }
        [[nodiscard]] bool   depth_write()     const noexcept { return m_depth_write; }
        [[nodiscard]] GLenum depth_func()      const noexcept { return m_depth_func;  }

        // Estado de blend (attachment 0)
        [[nodiscard]] bool   blend_enabled()   const noexcept { return m_blend_enabled; }
        [[nodiscard]] GLenum src_RGB()         const noexcept { return m_src_RGB;      }
        [[nodiscard]] GLenum dst_RGB()         const noexcept { return m_dst_RGB;      }
        [[nodiscard]] GLenum blend_eq_RGB()    const noexcept { return m_blend_eq_RGB; }
        [[nodiscard]] GLenum src_A()           const noexcept { return m_src_A;        }
        [[nodiscard]] GLenum dst_A()           const noexcept { return m_dst_A;        }
        [[nodiscard]] GLenum blend_eq_A()      const noexcept { return m_blend_eq_A;   }

        // Descriptor layout — usado para mapear los puntos de binding en tiempo de draw
        [[nodiscard]] const rhi::DescriptorSetLayout& descriptor_layout() const noexcept
        { return m_desc_layout; }

        // Formato de vértice — aplicado por cada draw por GLCommandBuffer::bind_vertex_buffer()
        [[nodiscard]] const std::vector<rhi::VertexAttribute>& vertex_attribs() const noexcept
        { return m_vertex_attribs; }
        [[nodiscard]] uint32_t vertex_stride() const noexcept { return m_vertex_stride; }

        // Setters mutables — usados solo durante create_pipeline()
        void set_program (GLuint p)          noexcept { m_program  = p; }
        void set_vao     (GLuint v)          noexcept { m_vao      = v; }
        void set_name    (std::string n)     noexcept { m_name     = std::move(n); }
        void set_topology(GLenum t)          noexcept { m_topology = t; }

        void set_rasterizer_state(bool cull_enabled, GLenum cull_face, bool front_face_CCW, GLenum fill_mode) noexcept {
            m_cull_enabled   = cull_enabled;
            m_cull_face      = cull_face;
            m_front_face_CCW = front_face_CCW;
            m_fill_mode      = fill_mode;
        }

        void set_depth_state(bool test, bool write, GLenum func) noexcept {
            m_depth_test  = test;
            m_depth_write = write;
            m_depth_func  = func;
        }

        void set_blend_state(bool enabled,
                             GLenum src_RGB, GLenum dst_RGB, GLenum eq_RGB,
                             GLenum src_A,   GLenum dst_A,   GLenum eq_A) noexcept {
            m_blend_enabled = enabled;
            m_src_RGB       = src_RGB;  m_dst_RGB     = dst_RGB;  m_blend_eq_RGB = eq_RGB;
            m_src_A         = src_A;    m_dst_A       = dst_A;    m_blend_eq_A   = eq_A;
        }

        void set_descriptor_layout(rhi::DescriptorSetLayout l) noexcept
        { m_desc_layout = std::move(l); }

        void set_vertex_format(std::vector<rhi::VertexAttribute> attribs, uint32_t stride) noexcept {
            m_vertex_attribs = std::move(attribs);
            m_vertex_stride  = stride;
        }

    private:
        GLuint      m_program  = 0;
        GLuint      m_vao      = 0;
        std::string m_name;
        GLenum      m_topology = GL_TRIANGLES;

        // Rasterizador
        bool   m_cull_enabled   = true;
        GLenum m_cull_face      = GL_BACK;
        bool   m_front_face_CCW = true;
        GLenum m_fill_mode      = GL_FILL;

        // Depth / stencil
        bool   m_depth_test  = false;
        bool   m_depth_write = false;
        GLenum m_depth_func  = GL_LESS;

        // Blend (attachment 0)
        bool   m_blend_enabled = false;
        GLenum m_src_RGB       = GL_ONE;
        GLenum m_dst_RGB       = GL_ZERO;
        GLenum m_blend_eq_RGB  = GL_FUNC_ADD;
        GLenum m_src_A         = GL_ONE;
        GLenum m_dst_A         = GL_ZERO;
        GLenum m_blend_eq_A    = GL_FUNC_ADD;

        rhi::DescriptorSetLayout           m_desc_layout;
        std::vector<rhi::VertexAttribute>  m_vertex_attribs;
        uint32_t                           m_vertex_stride = 0;
    };

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
