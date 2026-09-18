#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

// Resolución de headers de GL — se prefiere GLAD, con fallback a los headers del sistema de la
// plataforma -----------------------------------------------------------------------------------
#if defined(ANXIETY_USE_GLAD)
// En Windows, se incluye <windows.h> antes de glad para que APIENTRY ya sea __stdcall cuando se
// procesa glad.h. Sin esto, glad define APIENTRY como vacío y windows.h lo redefine después → C4005.
#  if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include <windows.h>
#  endif
#  include <glad/glad.h>
#else
#  if defined(_WIN32)
#    include <GL/glcorearb.h>
#    include <GL/wglext.h>
#  elif defined(__APPLE__)
#    include <OpenGL/gl3.h>
#    include <OpenGL/gl3ext.h>
#  else
#    include <GL/glcorearb.h>
#    include <GL/glext.h>
#  endif
#endif

#include "../../rhi/RHITypes.h"
#include "../../rhi/PipelineDesc.h"
#include "../../rhi/VertexLayout.h"
#include "../../rhi/SamplerDesc.h"

namespace anxiety::rendering::backend::opengl {

    // Traducción de formatos -----------------------------------------------------------------------
    // Devuelve el formato interno con tamaño de GL (p. ej. GL_RGBA8) para una asignación de almacenamiento.
    inline GLenum to_GL_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return GL_RGBA8;   // GL no tiene formato interno BGRA8; se trata como RGBA8
        case rhi::Format::RGBA8_Unorm:       return GL_RGBA8;
        case rhi::Format::RGBA16_Float:      return GL_RGBA16F;
        case rhi::Format::D32_Float:         return GL_DEPTH_COMPONENT32F;
        case rhi::Format::D24_Unorm_S8_Uint: return GL_DEPTH24_STENCIL8;
        default:                             return GL_RGBA8;
        }
    }

    // Devuelve el formato base de GL (layout de píxeles) usado para subida/descarga.
    inline GLenum to_GL_base_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return GL_BGRA;
        case rhi::Format::RGBA8_Unorm:       return GL_RGBA;
        case rhi::Format::RGBA16_Float:      return GL_RGBA;
        case rhi::Format::D32_Float:         return GL_DEPTH_COMPONENT;
        case rhi::Format::D24_Unorm_S8_Uint: return GL_DEPTH_STENCIL;
        default:                             return GL_RGBA;
        }
    }

    // Devuelve el tipo de dato de píxel de GL usado para subida/descarga.
    inline GLenum to_GL_type(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return GL_UNSIGNED_BYTE;
        case rhi::Format::RGBA8_Unorm:       return GL_UNSIGNED_BYTE;
        case rhi::Format::RGBA16_Float:      return GL_HALF_FLOAT;
        case rhi::Format::D32_Float:         return GL_FLOAT;
        case rhi::Format::D24_Unorm_S8_Uint: return GL_UNSIGNED_INT_24_8;
        default:                             return GL_UNSIGNED_BYTE;
        }
    }

    inline bool is_depth_format(rhi::Format fmt) noexcept {
        return fmt == rhi::Format::D32_Float || fmt == rhi::Format::D24_Unorm_S8_Uint;
    }

    // Topología de primitivas ------------------------------------------------------------------
    inline GLenum to_GL_primitive_type(rhi::PrimitiveTopology t) noexcept {
        switch (t) {
        case rhi::PrimitiveTopology::TriangleList:  return GL_TRIANGLES;
        case rhi::PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case rhi::PrimitiveTopology::LineList:      return GL_LINES;
        case rhi::PrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
        case rhi::PrimitiveTopology::PointList:     return GL_POINTS;
        default:                                    return GL_TRIANGLES;
        }
    }

    // Depth / stencil ---------------------------------------------------------------------------
    inline GLenum to_GL_depth_func(rhi::CompareOp op) noexcept {
        switch (op) {
        case rhi::CompareOp::Never:        return GL_NEVER;
        case rhi::CompareOp::Less:         return GL_LESS;
        case rhi::CompareOp::Equal:        return GL_EQUAL;
        case rhi::CompareOp::LessEqual:    return GL_LEQUAL;
        case rhi::CompareOp::Greater:      return GL_GREATER;
        case rhi::CompareOp::NotEqual:     return GL_NOTEQUAL;
        case rhi::CompareOp::GreaterEqual: return GL_GEQUAL;
        case rhi::CompareOp::Always:       return GL_ALWAYS;
        default:                           return GL_LESS;
        }
    }

    // Estado de blend ---------------------------------------------------------------------------
    inline GLenum to_GL_blend_factor(rhi::BlendFactor f) noexcept {
        switch (f) {
        case rhi::BlendFactor::Zero:             return GL_ZERO;
        case rhi::BlendFactor::One:              return GL_ONE;
        case rhi::BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
        case rhi::BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case rhi::BlendFactor::DstAlpha:         return GL_DST_ALPHA;
        case rhi::BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case rhi::BlendFactor::SrcColor:         return GL_SRC_COLOR;
        case rhi::BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        default:                                 return GL_ONE;
        }
    }

    inline GLenum to_GL_blend_equation(rhi::BlendOp op) noexcept {
        switch (op) {
        case rhi::BlendOp::Add:             return GL_FUNC_ADD;
        case rhi::BlendOp::Subtract:        return GL_FUNC_SUBTRACT;
        case rhi::BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case rhi::BlendOp::Min:             return GL_MIN;
        case rhi::BlendOp::Max:             return GL_MAX;
        default:                            return GL_FUNC_ADD;
        }
    }

    // Formato de vértice -------------------------------------------------------------------------
    struct GLVertexTypeInfo {
        GLenum    type;         // GL_FLOAT, GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, etc.
        GLint     components;   // 1-4
        GLboolean normalized;   // GL_TRUE si los datos enteros deben remapearse a [0,1]
    };

    inline GLVertexTypeInfo to_GL_vertex_type(rhi::VertexFormat fmt) noexcept {
        switch (fmt) {
        case rhi::VertexFormat::Float:      return { GL_FLOAT,         1, GL_FALSE };
        case rhi::VertexFormat::Float2:     return { GL_FLOAT,         2, GL_FALSE };
        case rhi::VertexFormat::Float3:     return { GL_FLOAT,         3, GL_FALSE };
        case rhi::VertexFormat::Float4:     return { GL_FLOAT,         4, GL_FALSE };
        case rhi::VertexFormat::UInt:       return { GL_UNSIGNED_INT,  1, GL_FALSE };
        case rhi::VertexFormat::UInt2:      return { GL_UNSIGNED_INT,  2, GL_FALSE };
        case rhi::VertexFormat::UInt3:      return { GL_UNSIGNED_INT,  3, GL_FALSE };
        case rhi::VertexFormat::UInt4:      return { GL_UNSIGNED_INT,  4, GL_FALSE };
        case rhi::VertexFormat::UByte4:     return { GL_UNSIGNED_BYTE, 4, GL_FALSE };
        case rhi::VertexFormat::UByte4Norm: return { GL_UNSIGNED_BYTE, 4, GL_TRUE  };
        default:                            return { GL_FLOAT,         4, GL_FALSE };
        }
    }

    // Filtro / wrap del sampler -------------------------------------------------------------------
    // Devuelve el filtro de minificación de GL (con mipmapping).
    inline GLenum to_GL_texture_min_filter(rhi::FilterMode f) noexcept {
        switch (f) {
        case rhi::FilterMode::Nearest:     return GL_NEAREST_MIPMAP_NEAREST;
        case rhi::FilterMode::Anisotropic: return GL_LINEAR_MIPMAP_LINEAR;
        default:                           return GL_LINEAR_MIPMAP_LINEAR;
        }
    }

    // Devuelve el filtro de magnificación de GL.
    inline GLenum to_GL_texture_mag_filter(rhi::FilterMode f) noexcept {
        switch (f) {
        case rhi::FilterMode::Nearest: return GL_NEAREST;
        default:                       return GL_LINEAR;
        }
    }

    inline GLenum to_GL_texture_wrap(rhi::AddressMode m) noexcept {
        switch (m) {
        case rhi::AddressMode::Clamp:  return GL_CLAMP_TO_EDGE;
        case rhi::AddressMode::Mirror: return GL_MIRRORED_REPEAT;
        default:                       return GL_REPEAT;
        }
    }

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
