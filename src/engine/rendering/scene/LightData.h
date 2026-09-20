#pragma once

#include <cstdint>
#include <type_traits>

// LightData.h — espejo en CPU del constant buffer de luces de la GPU -----------------------------
//
// GpuLightsCB se sube una vez por fotograma a LightsCB (registro b2), que lee el shader PBR. El
// layout debe coincidir exactamente con el cbuffer HLSL en assets/shaders/pbr.hlsl.
//
// Layout en memoria (576 bytes):
//   offset   0 – 15  : dirección de la luz direccional (float3) + intensidad (float)
//   offset  16 – 31  : color de la luz direccional (float3)     + padding (float)
//   offset  32 – 35  : numPointLights (int)
//   offset  36 – 47  : padding (float3)
//   offset  48 – 559 : point lights[16]  (32 bytes cada una)
//   offset 560 – 575 : posición de la cámara en espacio de mundo (float3) + padding (float)
//
// GpuPerObject sustituye al antiguo buffer de 64 bytes "solo WVP". Tiene 128 bytes para que tanto
// la WVP como la matriz de mundo (necesaria para transformar normales correctamente) quepan en una
// única asignación de CB de DX12 de 256 bytes.
// ------------------------------------------------------------------------------------------------

namespace anxiety::rendering::scene {
    // Constant buffer por objeto (b0) — 128 bytes -------------------------------------------------
    // world_view_proj : usado para transformar la posición del vértice (NDC).
    // world_matrix    : usado para transformar normales y posiciones a espacio de mundo.
    struct alignas(16) GpuPerObject {
        float world_view_proj[16];                          // offset   0: row-major 4×4
        float world_matrix[16];                             // offset  64: row-major 4×4
                                                            // Total: 128 bytes
    };
    static_assert(sizeof(GpuPerObject) == 128);
    static_assert(std::is_trivially_copyable_v<GpuPerObject>);

    // Una única luz puntual en la GPU (32 bytes) --------------------------------------------------
    struct alignas(16) GpuPointLight {
        float position[3];                                  // offset  0
        float intensity;                                    // offset 12
        float color[3];                                     // offset 16
        float range;                                        // offset 28
                                                            // Total: 32 bytes
    };
    static_assert(sizeof(GpuPointLight) == 32);
    static_assert(std::is_trivially_copyable_v<GpuPointLight>);

    // Constant buffer de luces completo (b2) — 576 bytes ------------------------------------------
    static constexpr int k_max_point_lights = 16;

    struct alignas(16) GpuLightsCB {
        // Luz direccional — offset 0
        float dir_direction[3]; float dir_intensity;        //  0 –  15
        float dir_color[3];     float dir_pad;              // 16 –  31

        // Recuento de luces puntuales y padding — offset 32
        int   num_point_lights;
        float _light_pad[3];                                // 32 –  47

        // Array de luces puntuales — offset 48
        GpuPointLight point_lights[k_max_point_lights];     // 48 – 559

        // Posición de la cámara — offset 560
        float camera_pos[3];
        float _pad2;                                        // 560 – 575
                                                            // Total: 576 bytes
    };
    static_assert(sizeof(GpuLightsCB) == 576);
    static_assert(std::is_trivially_copyable_v<GpuLightsCB>);
} // namespace anxiety::rendering::scene
