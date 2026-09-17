#pragma once

#include <cstdint>

namespace anxiety::rendering::rhi {
    // Modos de filtrado de textura -------------------------------------------------------------
    enum class FilterMode : uint32_t {
        Nearest,                                        // filtro de punto — sin mezcla entre téxeles
        Linear,                                         // filtro bilineal — mezcla suave
        Anisotropic                                     // filtro anisotrópico — mejor calidad, mayor coste
    };

    // Modos de direccionamiento (wrap) de textura --------------------------------------------
    enum class AddressMode : uint32_t {
        Wrap,                                           // repite la textura — la UV se envuelve en cada frontera entera
        Clamp,                                          // fija al borde — la UV se recorta a [0, 1]
        Mirror                                          // repetición en espejo en cada frontera entera
    };

    // SamplerDesc --------------------------------------------------------------------------------
    // Describe un sampler estático incrustado en el root signature del pipeline. Los samplers
    // estáticos no consumen espacio en el descriptor heap y encajan con el caso habitual de
    // samplers que no cambian por dibujado.
    //
    // shader_register se corresponde con s<N> en HLSL. Varios samplers en un mismo PipelineDesc
    // deben usar valores de shader_register distintos.
    struct SamplerDesc {
        FilterMode  filter          = FilterMode::Linear;
        AddressMode address_U       = AddressMode::Wrap;
        AddressMode address_V       = AddressMode::Wrap;
        AddressMode address_W       = AddressMode::Wrap;
        uint32_t    shader_register = 0;                // índice del registro sN
        float       mip_lod_bias    = 0.0f;
        float       min_lod         = 0.0f;
        float       max_lod         = 1000.0f;
        uint32_t    max_anisotropy  = 16;               // usado cuando filter == Anisotropic
    };

    // Funciones factoría de conveniencia -----------------------------------------------------
    inline SamplerDesc sampler_linear_wrap(uint32_t reg = 0)  { return { FilterMode::Linear, AddressMode::Wrap, AddressMode::Wrap, AddressMode::Wrap, reg }; }
    inline SamplerDesc sampler_linear_clamp(uint32_t reg = 0) { return { FilterMode::Linear, AddressMode::Clamp, AddressMode::Clamp, AddressMode::Clamp, reg }; }
    inline SamplerDesc sampler_nearest_wrap(uint32_t reg = 0) { return { FilterMode::Nearest, AddressMode::Wrap, AddressMode::Wrap, AddressMode::Wrap, reg }; }
} // namespace anxiety::rendering::rhi
