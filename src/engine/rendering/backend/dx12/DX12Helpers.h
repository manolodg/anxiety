#pragma once

#include "../../rhi/RHITypes.h"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace anxiety::rendering::backend::dx12 {
    // Traducción de estados / formatos — RHI ↔ D3D12 / DXGI ------------------------------------------

    inline D3D12_RESOURCE_STATES to_D3D12_state(anxiety::rendering::rhi::ResourceState state) noexcept {
        switch (state) {
        case anxiety::rendering::rhi::ResourceState::Undefined:
        case anxiety::rendering::rhi::ResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
        case anxiety::rendering::rhi::ResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case anxiety::rendering::rhi::ResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
        case anxiety::rendering::rhi::ResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case anxiety::rendering::rhi::ResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case anxiety::rendering::rhi::ResourceState::CopyDest:        return D3D12_RESOURCE_STATE_COPY_DEST;
        case anxiety::rendering::rhi::ResourceState::CopySrc:         return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case anxiety::rendering::rhi::ResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case anxiety::rendering::rhi::ResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
        default:                                                      return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    inline DXGI_FORMAT to_D3D12_format(anxiety::rendering::rhi::Format fmt) noexcept {
        switch (fmt) {
        case anxiety::rendering::rhi::Format::BGRA8_Unorm:            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA8_Unorm:            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA16_Float:           return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case anxiety::rendering::rhi::Format::D32_Float:              return DXGI_FORMAT_D32_FLOAT;
        case anxiety::rendering::rhi::Format::D24_Unorm_S8_Uint:      return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                                                      return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline anxiety::rendering::rhi::Format from_D3D12_format(DXGI_FORMAT fmt) noexcept {
        switch (fmt) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:                              return anxiety::rendering::rhi::Format::BGRA8_Unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:                              return anxiety::rendering::rhi::Format::RGBA8_Unorm;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:                          return anxiety::rendering::rhi::Format::RGBA16_Float;
        case DXGI_FORMAT_D32_FLOAT:                                   return anxiety::rendering::rhi::Format::D32_Float;
        default:                                                      return anxiety::rendering::rhi::Format::Unknown;
        }
    }
} // namespace anxiety::rendering::backend::dx12
