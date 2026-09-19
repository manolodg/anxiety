#pragma once

#include "../rhi/ShaderTypes.h"

#include <cstdint>
#include <vector>

namespace anxiety::rendering::shader {

    // Clase de registro HLSL → número de binding en SPIR-V --------------------------------------------
    // Los shaders del motor se escriben para D3D12 (bN, tN, sN, uN pueden repetir número entre
    // clases). En SPIR-V los bindings deben ser únicos dentro de un set, así que cada clase se
    // desplaza a su propio rango. VulkanHelpers::to_vk_binding_index() y el backend de OpenGL
    // (al deshacer el desplazamiento) DEBEN usar estas mismas bases.
    inline constexpr uint32_t k_cbv_binding_base     = 0;    // cbuffer bN      -> binding [0..15]
    inline constexpr uint32_t k_srv_binding_base     = 16;   // Texture2D tN    -> binding [16..31]
    inline constexpr uint32_t k_sampler_binding_base = 32;   // SamplerState sN -> binding [32..47]
    inline constexpr uint32_t k_uav_binding_base     = 48;   // RWBuffer uN     -> binding [48..63]

    // compile_hlsl_to_spirv ----------------------------------------------------------------------------
    // HLSL -> SPIR-V (Vulkan 1.2) vía shaderc. Devuelve los bytes del módulo SPIR-V, o un vector
    // vacío si falla o si el motor se compiló sin shaderc (ANXIETY_HAVE_SHADERC). Lo comparten el
    // backend de Vulkan (usa el SPIR-V tal cual) y el de OpenGL (lo traduce a GLSL con SPIRV-Cross).
    [[nodiscard]] std::vector<uint8_t> compile_hlsl_to_spirv(const char* source, const char* entry_point, rhi::ShaderStage stage);

} // namespace anxiety::rendering::shader
