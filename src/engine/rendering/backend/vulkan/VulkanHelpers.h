#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/RHITypes.h"

#include <vulkan/vulkan.h>

namespace anxiety::rendering::backend::vulkan {
    // Format translation -------------------------------------------------------------------------
    inline VkFormat to_vk_format(anxiety::rendering::rhi::Format fmt) noexcept {
        switch (fmt) {
        case anxiety::rendering::rhi::Format::BGRA8_Unorm:       return VK_FORMAT_B8G8R8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA8_Unorm:       return VK_FORMAT_R8G8B8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA16_Float:      return VK_FORMAT_R16G16B16A16_SFLOAT;
        case anxiety::rendering::rhi::Format::D32_Float:         return VK_FORMAT_D32_SFLOAT;
        case anxiety::rendering::rhi::Format::D24_Unorm_S8_Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
        default:                                                 return VK_FORMAT_UNDEFINED;
        }
    }

    inline anxiety::rendering::rhi::Format from_vk_format(VkFormat fmt) noexcept {
        switch (fmt) {
        case VK_FORMAT_B8G8R8A8_UNORM:      return anxiety::rendering::rhi::Format::BGRA8_Unorm;
        case VK_FORMAT_R8G8B8A8_UNORM:      return anxiety::rendering::rhi::Format::RGBA8_Unorm;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return anxiety::rendering::rhi::Format::RGBA16_Float;
        case VK_FORMAT_D32_SFLOAT:          return anxiety::rendering::rhi::Format::D32_Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return anxiety::rendering::rhi::Format::D24_Unorm_S8_Uint;
        default:                            return anxiety::rendering::rhi::Format::Unknown;
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
    inline VkImageLayout to_vk_image_layout(anxiety::rendering::rhi::ResourceState state) noexcept {
        using RS = anxiety::rendering::rhi::ResourceState;
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
    inline VkAccessFlags to_vk_access_flags(anxiety::rendering::rhi::ResourceState state) noexcept {
        using RS = anxiety::rendering::rhi::ResourceState;
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
    inline VkPipelineStageFlags to_vk_stage_flags(anxiety::rendering::rhi::ResourceState state) noexcept {
        using RS = anxiety::rendering::rhi::ResourceState;
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
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
