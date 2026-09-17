#ifdef ANXIETY_BACKEND_DX12

#pragma once

#include "../../rhi/RHITypes.h"
#include "../../rhi/VertexLayout.h"
#include "../../rhi/PipelineDesc.h"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace anxiety::rendering::backend::dx12 {
    // Traducción de estados / formatos — RHI ↔ D3D12 / DXGI ------------------------------------------

    inline D3D12_RESOURCE_STATES to_D3D12_state(rhi::ResourceState state) noexcept {
        switch (state) {
        case rhi::ResourceState::Undefined:
        case rhi::ResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
        case rhi::ResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case rhi::ResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
        case rhi::ResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case rhi::ResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case rhi::ResourceState::CopyDest:        return D3D12_RESOURCE_STATE_COPY_DEST;
        case rhi::ResourceState::CopySrc:         return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case rhi::ResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case rhi::ResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
        default:                                  return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    inline DXGI_FORMAT to_D3D12_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case rhi::Format::RGBA8_Unorm:            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case rhi::Format::RGBA16_Float:           return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case rhi::Format::D32_Float:              return DXGI_FORMAT_D32_FLOAT;
        case rhi::Format::D24_Unorm_S8_Uint:      return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                                  return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline rhi::Format from_D3D12_format(DXGI_FORMAT fmt) noexcept {
        switch (fmt) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:                              return rhi::Format::BGRA8_Unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:                              return rhi::Format::RGBA8_Unorm;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:                          return rhi::Format::RGBA16_Float;
        case DXGI_FORMAT_D32_FLOAT:                                   return rhi::Format::D32_Float;
        default:                                                      return rhi::Format::Unknown;
        }
    }

    inline DXGI_FORMAT to_D3D12_vertex_format(rhi::VertexFormat fmt) noexcept {
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

    inline D3D12_PRIMITIVE_TOPOLOGY_TYPE to_D3D12_topology_type(rhi::PrimitiveTopology t) noexcept {
        switch (t) {
        case rhi::PrimitiveTopology::TriangleList:
        case rhi::PrimitiveTopology::TriangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case rhi::PrimitiveTopology::LineList:
        case rhi::PrimitiveTopology::LineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case rhi::PrimitiveTopology::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }

    inline D3D_PRIMITIVE_TOPOLOGY to_D3D12_topology(rhi::PrimitiveTopology t) noexcept {
        switch (t) {
        case rhi::PrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case rhi::PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case rhi::PrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case rhi::PrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case rhi::PrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                                    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

    inline D3D12_FILL_MODE to_D3D12_fill_mode(rhi::FillMode m) noexcept {
        return m == rhi::FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    }

    inline D3D12_CULL_MODE to_D3D12_cull_mode(rhi::CullMode m) noexcept {
        switch (m) {
        case rhi::CullMode::None:  return D3D12_CULL_MODE_NONE;
        case rhi::CullMode::Front: return D3D12_CULL_MODE_FRONT;
        case rhi::CullMode::Back:  return D3D12_CULL_MODE_BACK;
        default:                   return D3D12_CULL_MODE_BACK;
        }
    }

    inline D3D12_COMPARISON_FUNC to_D3D12_compare_op(rhi::CompareOp op) noexcept {
        switch (op) {
        case rhi::CompareOp::Never:        return D3D12_COMPARISON_FUNC_NEVER;
        case rhi::CompareOp::Less:         return D3D12_COMPARISON_FUNC_LESS;
        case rhi::CompareOp::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
        case rhi::CompareOp::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case rhi::CompareOp::Greater:      return D3D12_COMPARISON_FUNC_GREATER;
        case rhi::CompareOp::NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case rhi::CompareOp::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case rhi::CompareOp::Always:       return D3D12_COMPARISON_FUNC_ALWAYS;
        default:                           return D3D12_COMPARISON_FUNC_LESS;
        }
    }

    inline D3D12_BLEND to_D3D12_blend_factor(rhi::BlendFactor f) noexcept {
        switch (f) {
        case rhi::BlendFactor::Zero:             return D3D12_BLEND_ZERO;
        case rhi::BlendFactor::One:              return D3D12_BLEND_ONE;
        case rhi::BlendFactor::SrcAlpha:         return D3D12_BLEND_SRC_ALPHA;
        case rhi::BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case rhi::BlendFactor::DstAlpha:         return D3D12_BLEND_DEST_ALPHA;
        case rhi::BlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
        case rhi::BlendFactor::SrcColor:         return D3D12_BLEND_SRC_COLOR;
        case rhi::BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
        default:                                 return D3D12_BLEND_ONE;
        }
    }

    inline D3D12_BLEND_OP to_D3D12_blend_op(rhi::BlendOp op) noexcept {
        switch (op) {
        case rhi::BlendOp::Add:             return D3D12_BLEND_OP_ADD;
        case rhi::BlendOp::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
        case rhi::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case rhi::BlendOp::Min:             return D3D12_BLEND_OP_MIN;
        case rhi::BlendOp::Max:             return D3D12_BLEND_OP_MAX;
        default:                            return D3D12_BLEND_OP_ADD;
        }
    }

    // Helpers de conversión de sampler ---------------------------------------------------------
    inline D3D12_FILTER to_D3D12_filter(rhi::FilterMode f) noexcept {
        switch (f) {
        case rhi::FilterMode::Nearest:     return D3D12_FILTER_MIN_MAG_MIP_POINT;
        case rhi::FilterMode::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
        default:                           return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    inline D3D12_TEXTURE_ADDRESS_MODE to_D3D12_address_mode(rhi::AddressMode m) noexcept {
        switch (m) {
        case rhi::AddressMode::Clamp:  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case rhi::AddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        default:                       return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    inline D3D12_STATIC_SAMPLER_DESC to_D3D12_static_sampler(const rhi::SamplerDesc& sd, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL) noexcept {
        D3D12_STATIC_SAMPLER_DESC d{};
        d.Filter           = to_D3D12_filter(sd.filter);
        d.AddressU         = to_D3D12_address_mode(sd.address_U);
        d.AddressV         = to_D3D12_address_mode(sd.address_V);
        d.AddressW         = to_D3D12_address_mode(sd.address_W);
        d.MipLODBias       = sd.mip_lod_bias;
        d.MaxAnisotropy    = sd.max_anisotropy;
        d.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        d.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        d.MinLOD           = sd.min_lod;
        d.MaxLOD           = sd.max_lod;
        d.ShaderRegister   = sd.shader_register;
        d.RegisterSpace    = 0;
        d.ShaderVisibility = vis;

        return d;
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12
