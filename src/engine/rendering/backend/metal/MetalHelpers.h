#ifdef ANXIETY_BACKEND_METAL

#pragma once

#import <Metal/Metal.h>

#include "../../rhi/RHITypes.h"
#include "../../rhi/PipelineDesc.h"
#include "../../rhi/SamplerDesc.h"
#include "../../rhi/VertexLayout.h"

namespace anxiety::rendering::backend::metal {
    // Formato de píxel -----------------------------------------------------------------------------
    inline MTLPixelFormat to_MTL_pixel_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return MTLPixelFormatBGRA8Unorm;
        case rhi::Format::RGBA8_Unorm:       return MTLPixelFormatRGBA8Unorm;
        case rhi::Format::RGBA16_Float:      return MTLPixelFormatRGBA16Float;
        case rhi::Format::D32_Float:         return MTLPixelFormatDepth32Float;
        case rhi::Format::D24_Unorm_S8_Uint: return MTLPixelFormatDepth32Float_Stencil8; // equivalente más cercano en Metal
        default:                             return MTLPixelFormatInvalid;
        }
    }

    // Acciones de carga / almacenamiento ------------------------------------------------------------
    inline MTLLoadAction  to_MTL_load_action(bool clear) noexcept { return clear ? MTLLoadActionClear : MTLLoadActionLoad; }
    inline MTLStoreAction to_MTL_store_action()          noexcept { return MTLStoreActionStore; }


    // Topología de primitivas ------------------------------------------------------------------------
    inline MTLPrimitiveType to_MTL_primitive_type(rhi::PrimitiveTopology t) noexcept {
        switch (t) {
        case rhi::PrimitiveTopology::TriangleList:  return MTLPrimitiveTypeTriangle;
        case rhi::PrimitiveTopology::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case rhi::PrimitiveTopology::LineList:      return MTLPrimitiveTypeLine;
        case rhi::PrimitiveTopology::LineStrip:     return MTLPrimitiveTypeLineStrip;
        case rhi::PrimitiveTopology::PointList:     return MTLPrimitiveTypePoint;
        default:                                    return MTLPrimitiveTypeTriangle;
        }
    }

    // Rasterizador -------------------------------------------------------------------------------------
    inline MTLTriangleFillMode to_MTL_triangle_fill_mode(rhi::FillMode m) noexcept {
        return m == rhi::FillMode::Wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
    }

    inline MTLCullMode to_MTL_cull_mode(rhi::CullMode m) noexcept {
        switch (m) {
        case rhi::CullMode::None:  return MTLCullModeNone;
        case rhi::CullMode::Front: return MTLCullModeFront;
        case rhi::CullMode::Back:  return MTLCullModeBack;
        default:                   return MTLCullModeBack;
        }
    }

    // front_CCW = true  → cara frontal antihoraria (convención OpenGL/anxiety)
    // front_CCW = false → cara frontal horaria
    inline MTLWinding to_MTL_winding(bool front_CCW) noexcept {
        return front_CCW ? MTLWindingCounterClockwise : MTLWindingClockwise;
    }

    // Comparación de profundidad ---------------------------------------------------------------------
    inline MTLCompareFunction to_MTL_compare_function(rhi::CompareOp op) noexcept {
        switch (op) {
        case rhi::CompareOp::Never:        return MTLCompareFunctionNever;
        case rhi::CompareOp::Less:         return MTLCompareFunctionLess;
        case rhi::CompareOp::Equal:        return MTLCompareFunctionEqual;
        case rhi::CompareOp::LessEqual:    return MTLCompareFunctionLessEqual;
        case rhi::CompareOp::Greater:      return MTLCompareFunctionGreater;
        case rhi::CompareOp::NotEqual:     return MTLCompareFunctionNotEqual;
        case rhi::CompareOp::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case rhi::CompareOp::Always:       return MTLCompareFunctionAlways;
        default:                           return MTLCompareFunctionLess;
        }
    }

    // Mezcla (blend) -----------------------------------------------------------------------------------
    inline MTLBlendFactor to_MTL_blend_factor(rhi::BlendFactor f) noexcept {
        switch (f) {
        case rhi::BlendFactor::Zero:             return MTLBlendFactorZero;
        case rhi::BlendFactor::One:              return MTLBlendFactorOne;
        case rhi::BlendFactor::SrcAlpha:         return MTLBlendFactorSourceAlpha;
        case rhi::BlendFactor::OneMinusSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
        case rhi::BlendFactor::DstAlpha:         return MTLBlendFactorDestinationAlpha;
        case rhi::BlendFactor::OneMinusDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
        case rhi::BlendFactor::SrcColor:         return MTLBlendFactorSourceColor;
        case rhi::BlendFactor::OneMinusSrcColor: return MTLBlendFactorOneMinusSourceColor;
        default:                                 return MTLBlendFactorOne;
        }
    }

    inline MTLBlendOperation to_MTL_blend_operation(rhi::BlendOp op) noexcept {
        switch (op) {
        case rhi::BlendOp::Add:             return MTLBlendOperationAdd;
        case rhi::BlendOp::Subtract:        return MTLBlendOperationSubtract;
        case rhi::BlendOp::ReverseSubtract: return MTLBlendOperationReverseSubtract;
        case rhi::BlendOp::Min:             return MTLBlendOperationMin;
        case rhi::BlendOp::Max:             return MTLBlendOperationMax;
        default:                            return MTLBlendOperationAdd;
        }
    }

    // Formato de vértice -------------------------------------------------------------------------------
    inline MTLVertexFormat to_MTL_vertex_format(rhi::VertexFormat fmt) noexcept {
        switch (fmt) {
        case rhi::VertexFormat::Float:      return MTLVertexFormatFloat;
        case rhi::VertexFormat::Float2:     return MTLVertexFormatFloat2;
        case rhi::VertexFormat::Float3:     return MTLVertexFormatFloat3;
        case rhi::VertexFormat::Float4:     return MTLVertexFormatFloat4;
        case rhi::VertexFormat::UInt:       return MTLVertexFormatUInt;
        case rhi::VertexFormat::UInt2:      return MTLVertexFormatUInt2;
        case rhi::VertexFormat::UInt3:      return MTLVertexFormatUInt3;
        case rhi::VertexFormat::UInt4:      return MTLVertexFormatUInt4;
        case rhi::VertexFormat::UByte4:     return MTLVertexFormatUChar4;
        case rhi::VertexFormat::UByte4Norm: return MTLVertexFormatUChar4Normalized;
        default:                            return MTLVertexFormatFloat4;
        }
    }

    // Sampler ------------------------------------------------------------------------------------------
    inline MTLSamplerMinMagFilter to_MTL_sampler_min_mag_filter(rhi::FilterMode f) noexcept {
        switch (f) {
        case rhi::FilterMode::Nearest:
            return MTLSamplerMinMagFilterNearest;
        case rhi::FilterMode::Linear:
        case rhi::FilterMode::Anisotropic:
        default:
            return MTLSamplerMinMagFilterLinear;
        }
    }

    inline MTLSamplerAddressMode to_MTL_sampler_address_mode(rhi::AddressMode m) noexcept {
        switch (m) {
        case rhi::AddressMode::Clamp:  return MTLSamplerAddressModeClampToEdge;
        case rhi::AddressMode::Mirror: return MTLSamplerAddressModeMirrorRepeat;
        case rhi::AddressMode::Wrap:
        default:                       return MTLSamplerAddressModeRepeat;
        }
    }
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
