#ifdef ANXIETY_BACKEND_DX11

#pragma once
#include "../../rhi/RHITypes.h"
#include "../../rhi/VertexLayout.h"
#include "../../rhi/PipelineDesc.h"
#include "../../rhi/SamplerDesc.h"

#include <d3d11.h>
#include <dxgi1_2.h>

namespace anxiety::rendering::backend::dx11 {
    // Traducción de formatos — RHI ↔ DXGI / D3D11 ----------------------------------------------------
    inline DXGI_FORMAT to_DX11_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
        case rhi::Format::RGBA8_Unorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case rhi::Format::RGBA16_Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case rhi::Format::D32_Float:         return DXGI_FORMAT_D32_FLOAT;
        case rhi::Format::D24_Unorm_S8_Uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                             return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline rhi::Format from_DX11_format(DXGI_FORMAT fmt) noexcept {
        switch (fmt) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:     return rhi::Format::BGRA8_Unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:     return rhi::Format::RGBA8_Unorm;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return rhi::Format::RGBA16_Float;
        case DXGI_FORMAT_D32_FLOAT:          return rhi::Format::D32_Float;
        default:                             return rhi::Format::Unknown;
        }
    }

    inline DXGI_FORMAT to_DX11_vertex_format(rhi::VertexFormat fmt) noexcept {
        switch (fmt) {
        case rhi::VertexFormat::Float:      return DXGI_FORMAT_R32_FLOAT;
        case rhi::VertexFormat::Float2:     return DXGI_FORMAT_R32G32_FLOAT;
        case rhi::VertexFormat::Float3:     return DXGI_FORMAT_R32G32B32_FLOAT;
        case rhi::VertexFormat::Float4:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case rhi::VertexFormat::UInt:       return DXGI_FORMAT_R32_UINT;
        case rhi::VertexFormat::UInt2:      return DXGI_FORMAT_R32G32_UINT;
        case rhi::VertexFormat::UInt3:      return DXGI_FORMAT_R32G32B32_UINT;
        case rhi::VertexFormat::UInt4:      return DXGI_FORMAT_R32G32B32A32_UINT;
        case rhi::VertexFormat::UByte4:     return DXGI_FORMAT_R8G8B8A8_UINT;
        case rhi::VertexFormat::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        default:                            return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline D3D11_PRIMITIVE_TOPOLOGY to_DX11_topology(rhi::PrimitiveTopology t) noexcept {
        switch (t) {
        case rhi::PrimitiveTopology::TriangleList:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case rhi::PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case rhi::PrimitiveTopology::LineList:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case rhi::PrimitiveTopology::LineStrip:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case rhi::PrimitiveTopology::PointList:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                                    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

    inline D3D11_FILL_MODE to_DX11_fill_mode(rhi::FillMode m) noexcept {
        return m == rhi::FillMode::Wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
    }

    inline D3D11_CULL_MODE to_DX11_cull_mode(rhi::CullMode m) noexcept {
        switch (m) {
        case rhi::CullMode::None:  return D3D11_CULL_NONE;
        case rhi::CullMode::Front: return D3D11_CULL_FRONT;
        case rhi::CullMode::Back:  return D3D11_CULL_BACK;
        default:                   return D3D11_CULL_BACK;
        }
    }

    inline D3D11_COMPARISON_FUNC to_DX11_compare_func(rhi::CompareOp op) noexcept {
        switch (op) {
        case rhi::CompareOp::Never:        return D3D11_COMPARISON_NEVER;
        case rhi::CompareOp::Less:         return D3D11_COMPARISON_LESS;
        case rhi::CompareOp::Equal:        return D3D11_COMPARISON_EQUAL;
        case rhi::CompareOp::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case rhi::CompareOp::Greater:      return D3D11_COMPARISON_GREATER;
        case rhi::CompareOp::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case rhi::CompareOp::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case rhi::CompareOp::Always:       return D3D11_COMPARISON_ALWAYS;
        default:                           return D3D11_COMPARISON_LESS;
        }
    }

    inline D3D11_BLEND to_DX11_blend_factor(rhi::BlendFactor f) noexcept {
        switch (f) {
        case rhi::BlendFactor::Zero:             return D3D11_BLEND_ZERO;
        case rhi::BlendFactor::One:              return D3D11_BLEND_ONE;
        case rhi::BlendFactor::SrcAlpha:         return D3D11_BLEND_SRC_ALPHA;
        case rhi::BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case rhi::BlendFactor::DstAlpha:         return D3D11_BLEND_DEST_ALPHA;
        case rhi::BlendFactor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
        case rhi::BlendFactor::SrcColor:         return D3D11_BLEND_SRC_COLOR;
        case rhi::BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
        default:                                 return D3D11_BLEND_ONE;
        }
    }

    inline D3D11_BLEND_OP to_DX11_blend_op(rhi::BlendOp op) noexcept {
        switch (op) {
        case rhi::BlendOp::Add:             return D3D11_BLEND_OP_ADD;
        case rhi::BlendOp::Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case rhi::BlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case rhi::BlendOp::Min:             return D3D11_BLEND_OP_MIN;
        case rhi::BlendOp::Max:             return D3D11_BLEND_OP_MAX;
        default:                            return D3D11_BLEND_OP_ADD;
        }
    }

    inline D3D11_FILTER to_DX11_filter(rhi::FilterMode f) noexcept {
        switch (f) {
        case rhi::FilterMode::Nearest:     return D3D11_FILTER_MIN_MAG_MIP_POINT;
        case rhi::FilterMode::Anisotropic: return D3D11_FILTER_ANISOTROPIC;
        default:                           return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    inline D3D11_TEXTURE_ADDRESS_MODE to_DX11_address_mode(rhi::AddressMode m) noexcept {
        switch (m) {
        case rhi::AddressMode::Clamp:  return D3D11_TEXTURE_ADDRESS_CLAMP;
        case rhi::AddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
        default:                       return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }
} // namespace anxiety::rendering::backend::dx11

#endif ANXIETY_BACKEND_DX11