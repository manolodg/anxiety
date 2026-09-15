#pragma once
#include "../../rhi/RHITypes.h"

#include <d3d11.h>
#include <dxgi1_2.h>

namespace anxiety::rendering::backend::dx11 {
    // Traducción de formatos — RHI ↔ DXGI / D3D11 ----------------------------------------------------
    inline DXGI_FORMAT to_DX11_format(anxiety::rendering::rhi::Format fmt) noexcept {
        switch (fmt) {
        case anxiety::rendering::rhi::Format::BGRA8_Unorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA8_Unorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case anxiety::rendering::rhi::Format::RGBA16_Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case anxiety::rendering::rhi::Format::D32_Float:         return DXGI_FORMAT_D32_FLOAT;
        case anxiety::rendering::rhi::Format::D24_Unorm_S8_Uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                                                 return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline anxiety::rendering::rhi::Format from_DX11_format(DXGI_FORMAT fmt) noexcept {
        switch (fmt) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:     return anxiety::rendering::rhi::Format::BGRA8_Unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:     return anxiety::rendering::rhi::Format::RGBA8_Unorm;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return anxiety::rendering::rhi::Format::RGBA16_Float;
        case DXGI_FORMAT_D32_FLOAT:          return anxiety::rendering::rhi::Format::D32_Float;
        default:                             return anxiety::rendering::rhi::Format::Unknown;
        }
    }
} // namespace anxiety::rendering::backend::dx11
