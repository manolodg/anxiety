#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/RHITypes.h"

#include <vulkan/vulkan.h>

namespace anxiety::rendering::backend::vulkan {
    // Clase de registro HLSL → número de binding de Vulkan -----------------------------------------
    // Nuestros shaders HLSL se escribieron para D3D12: los cbuffers usan bN, las texturas tN, los
    // samplers sN, las UAV uN — el mismo registro numérico puede repetirse legítimamente entre
    // clases (b0 y t0 pueden coexistir). Los bindings de descriptor set de Vulkan deben ser únicos
    // dentro de un set, así que cada clase de registro se desplaza a su propio rango numérico. Estas
    // bases DEBEN coincidir con las opciones de binding-base HLSL de shaderc usadas en
    // VulkanDevice::compile_shader_from_source() — ver k_cbv/srv/sampler/uav_binding_base más abajo.
    inline constexpr uint32_t k_cbv_binding_base = 0;    // cbuffer bN     -> binding [0..15]
    inline constexpr uint32_t k_srv_binding_base = 16;   // Texture2D tN   -> binding [16..31]
    inline constexpr uint32_t k_sampler_binding_base = 32;   // SamplerState sN-> binding [32..47]
    inline constexpr uint32_t k_uav_binding_base = 48;   // RWBuffer uN    -> binding [48..63]

    inline VkDescriptorType to_vk_descriptor_type(rhi::DescriptorType t) noexcept {
        switch (t) {
        case rhi::DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case rhi::DescriptorType::Texture:       return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case rhi::DescriptorType::Sampler:       return VK_DESCRIPTOR_TYPE_SAMPLER;
        case rhi::DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:                                 return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    }

    // Traduce un DescriptorBinding de la RHI (tipo + índice de registro HLSL) al número de binding
    // de Vulkan que shaderc le habrá asignado al recurso correspondiente en el SPIR-V compilado.
    inline uint32_t to_vk_binding_index(rhi::DescriptorType t, uint32_t register_index) noexcept {
        switch (t) {
        case rhi::DescriptorType::UniformBuffer: return k_cbv_binding_base + register_index;
        case rhi::DescriptorType::Texture:       return k_srv_binding_base + register_index;
        case rhi::DescriptorType::Sampler:       return k_sampler_binding_base + register_index;
        case rhi::DescriptorType::StorageBuffer: return k_uav_binding_base + register_index;
        default:                                 return k_cbv_binding_base + register_index;
        }
    }

    // Traducción de formato ----------------------------------------------------------------------
    inline VkFormat to_vk_format(rhi::Format fmt) noexcept {
        switch (fmt) {
        case rhi::Format::BGRA8_Unorm:       return VK_FORMAT_B8G8R8A8_UNORM;
        case rhi::Format::RGBA8_Unorm:       return VK_FORMAT_R8G8B8A8_UNORM;
        case rhi::Format::RGBA16_Float:      return VK_FORMAT_R16G16B16A16_SFLOAT;
        case rhi::Format::D32_Float:         return VK_FORMAT_D32_SFLOAT;
        case rhi::Format::D24_Unorm_S8_Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
        default:                             return VK_FORMAT_UNDEFINED;
        }
    }

    inline rhi::Format from_vk_format(VkFormat fmt) noexcept {
        switch (fmt) {
        case VK_FORMAT_B8G8R8A8_UNORM:      return rhi::Format::BGRA8_Unorm;
        case VK_FORMAT_R8G8B8A8_UNORM:      return rhi::Format::RGBA8_Unorm;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return rhi::Format::RGBA16_Float;
        case VK_FORMAT_D32_SFLOAT:          return rhi::Format::D32_Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return rhi::Format::D24_Unorm_S8_Uint;
        default:                            return rhi::Format::Unknown;
        }
    }

    inline bool is_depth_format(VkFormat fmt) noexcept {
        return fmt == VK_FORMAT_D32_SFLOAT
            || fmt == VK_FORMAT_D24_UNORM_S8_UINT
            || fmt == VK_FORMAT_D16_UNORM
            || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT
            || fmt == VK_FORMAT_D16_UNORM_S8_UINT;
    }

    // ResourceState → VkImageLayout --------------------------------------------------------------
    inline VkImageLayout to_vk_image_layout(rhi::ResourceState state) noexcept {
        using RS = rhi::ResourceState;
        switch (state) {
        case RS::Undefined:       return VK_IMAGE_LAYOUT_UNDEFINED;
        case RS::Common:          return VK_IMAGE_LAYOUT_GENERAL;
        case RS::RenderTarget:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case RS::Present:         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case RS::ShaderResource:  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case RS::UnorderedAccess: return VK_IMAGE_LAYOUT_GENERAL;
        case RS::CopyDest:        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case RS::CopySrc:         return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case RS::DepthWrite:      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case RS::DepthRead:       return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        default:                  return VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    // ResourceState → VkAccessFlags --------------------------------------------------------------
    inline VkAccessFlags to_vk_access_flags(rhi::ResourceState state) noexcept {
        using RS = rhi::ResourceState;
        switch (state) {
        case RS::Undefined:       return 0;
        case RS::Common:          return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        case RS::RenderTarget:    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case RS::Present:         return 0;
        case RS::ShaderResource:  return VK_ACCESS_SHADER_READ_BIT;
        case RS::UnorderedAccess: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case RS::CopyDest:        return VK_ACCESS_TRANSFER_WRITE_BIT;
        case RS::CopySrc:         return VK_ACCESS_TRANSFER_READ_BIT;
        case RS::DepthWrite:      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case RS::DepthRead:       return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        default:                  return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        }
    }

    // ResourceState → VkPipelineStageFlags -------------------------------------------------------
    inline VkPipelineStageFlags to_vk_stage_flags(rhi::ResourceState state) noexcept {
        using RS = rhi::ResourceState;
        switch (state) {
        case RS::Undefined:       return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case RS::RenderTarget:    return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case RS::Present:         return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case RS::ShaderResource:  return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case RS::UnorderedAccess: return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case RS::CopyDest:
        case RS::CopySrc:         return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case RS::DepthWrite:
        case RS::DepthRead:       return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        default:                  return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
    }

    // Formato de atributo de vértice -----------------------------------------------------------------
    inline VkFormat to_vk_vertex_format(rhi::VertexFormat fmt) noexcept {
        using VF = rhi::VertexFormat;
        switch (fmt) {
        case VF::Float:      return VK_FORMAT_R32_SFLOAT;
        case VF::Float2:     return VK_FORMAT_R32G32_SFLOAT;
        case VF::Float3:     return VK_FORMAT_R32G32B32_SFLOAT;
        case VF::Float4:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VF::UInt:       return VK_FORMAT_R32_UINT;
        case VF::UInt2:      return VK_FORMAT_R32G32_UINT;
        case VF::UInt3:      return VK_FORMAT_R32G32B32_UINT;
        case VF::UInt4:      return VK_FORMAT_R32G32B32A32_UINT;
        case VF::UByte4:     return VK_FORMAT_R8G8B8A8_UINT;
        case VF::UByte4Norm: return VK_FORMAT_R8G8B8A8_UNORM;
        default:             return VK_FORMAT_UNDEFINED;
        }
    }

    // Topología de primitivas -------------------------------------------------------------------------
    inline VkPrimitiveTopology to_vk_primitive_topology(rhi::PrimitiveTopology t) noexcept {
        using PT = rhi::PrimitiveTopology;
        switch (t) {
        case PT::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PT::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PT::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PT::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PT::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default:                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    // Rasterizador ---------------------------------------------------------------------------------
    inline VkPolygonMode to_vk_polygon_mode(rhi::FillMode m) noexcept {
        return m == rhi::FillMode::Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    }

    inline VkCullModeFlags to_vk_cull_mode(rhi::CullMode m) noexcept {
        switch (m) {
        case rhi::CullMode::None:  return VK_CULL_MODE_NONE;
        case rhi::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case rhi::CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default:                   return VK_CULL_MODE_BACK_BIT;
        }
    }

    // Profundidad / stencil ------------------------------------------------------------------------
    inline VkCompareOp to_vk_compare_op(rhi::CompareOp op) noexcept {
        using CO = rhi::CompareOp;
        switch (op) {
        case CO::Never:        return VK_COMPARE_OP_NEVER;
        case CO::Less:         return VK_COMPARE_OP_LESS;
        case CO::Equal:        return VK_COMPARE_OP_EQUAL;
        case CO::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CO::Greater:      return VK_COMPARE_OP_GREATER;
        case CO::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case CO::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CO::Always:       return VK_COMPARE_OP_ALWAYS;
        default:               return VK_COMPARE_OP_LESS;
        }
    }

    // Blend ----------------------------------------------------------------------------------------
    inline VkBlendFactor to_vk_blend_factor(rhi::BlendFactor f) noexcept {
        using BF = rhi::BlendFactor;
        switch (f) {
        case BF::Zero:             return VK_BLEND_FACTOR_ZERO;
        case BF::One:              return VK_BLEND_FACTOR_ONE;
        case BF::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
        case BF::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BF::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
        case BF::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BF::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
        case BF::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        default:                   return VK_BLEND_FACTOR_ONE;
        }
    }

    inline VkBlendOp to_vk_blend_op(rhi::BlendOp op) noexcept {
        using BO = rhi::BlendOp;
        switch (op) {
        case BO::Add:             return VK_BLEND_OP_ADD;
        case BO::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BO::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BO::Min:             return VK_BLEND_OP_MIN;
        case BO::Max:             return VK_BLEND_OP_MAX;
        default:                  return VK_BLEND_OP_ADD;
        }
    }

    // Sampler ----------------------------------------------------------------------------------------
    inline VkFilter to_vk_filter(rhi::FilterMode f) noexcept {
        return f == rhi::FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    }

    inline VkSamplerMipmapMode to_vk_mipmap_mode(rhi::FilterMode f) noexcept {
        return f == rhi::FilterMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    inline VkSamplerAddressMode to_vk_sampler_address_mode(rhi::AddressMode m) noexcept {
        switch (m) {
        case rhi::AddressMode::Clamp:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case rhi::AddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default:                       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
