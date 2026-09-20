#pragma once

#include "rhi/RHITypes.h"
#include "materials/MaterialHandle.h"

#include <cstdint>
#include <type_traits>

namespace anxiety::rendering::scene {
	// Transform ----------------------------------------------------------------------------------
	// Posición, orientación y escala de una entidad en espacio de mundo. La rotación se almacena
	// como un cuaternión unitario en orden xyzw (identidad = {0,0,0,1}).
	// --------------------------------------------------------------------------------------------
	struct Transform {
		float position[3] = { 0.0f, 0.0f, 0.0f };
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };         // xyzw, identidad
		float scale[3]    = { 1.0f, 1.0f, 1.0f };
	};
	static_assert(std::is_trivially_copyable_v<Transform>);

	// MeshRenderer -------------------------------------------------------------------------------
	// Referencias a la geometría de GPU de una entidad. Los buffers se crean vía
	// SceneRenderer::upload_mesh(); este componente guarda los handles resultantes y los
	// parámetros de dibujado.
	//
	// Formato de vértice esperado por el pipeline de escena:
	//   POSITION (float3, 12 bytes) | COLOR (float4, 16 bytes) — 28 bytes/vértice.
	// --------------------------------------------------------------------------------------------
	struct MeshRenderer {
        rhi::BufferHandle                 vertex_buffer;            // datos de vértice intercalados
        rhi::BufferHandle                 index_buffer;             // datos de índice (uint32_t)
        uint32_t                          index_count       = 0;
        uint32_t                          vertex_stride     = 28;   // bytes por vértice (pos+col por defecto)
		materials::MaterialInstanceHandle material_instance;        // inválido → material unlit por defecto
	};
	static_assert(std::is_trivially_copyable_v<MeshRenderer>);

	// Camera -------------------------------------------------------------------------------------
	// Parámetros de proyección en perspectiva para una entidad que actúa como cámara de la escena.
	// La transformación de vista se deriva del componente Transform de la entidad.
	// --------------------------------------------------------------------------------------------
	struct Camera {
        float fov_Y        = 1.0472f;               // 60° en radianes
        float near_Z       = 0.1f;
        float far_Z        = 1000.0f;
        float aspect_ratio = 16.0f / 9.0f;          // sobrescrito en tiempo de ejecución desde la extensión del swapchain
	};
	static_assert(std::is_trivially_copyable_v<Camera>);
    
    // DirectionalLight ---------------------------------------------------------------------------
    // Luz direccional infinita (p. ej. el sol). direction es un vector unitario que apunta DESDE la
    // fuente de luz HACIA la escena (es decir, la dirección en la que viaja la luz). El componente
    // Transform de la misma entidad se ignora a efectos de iluminación.
    struct DirectionalLight {
        float direction[3] = { 0.0f, -1.0f,  0.0f };                // apunta hacia abajo por defecto
        float intensity    = 1.0f;
        float color[3]     = { 1.0f,  1.0f,  1.0f };                // RGB lineal
        float _pad         = 0.0f;
    };
    static_assert(std::is_trivially_copyable_v<DirectionalLight>);

    // PointLight ---------------------------------------------------------------------------------
    // Luz puntual omnidireccional. La posición se toma del Transform de la entidad
    // (Transform::position), no se guarda aquí. range es el radio máximo de influencia en unidades
    // de mundo; la atenuación llega suavemente a cero en ese radio.
    struct PointLight {
        float color[3]  = { 1.0f, 1.0f, 1.0f };                     // RGB lineal
        float intensity = 1.0f;
        float range     = 10.0f;
        float _pad[3]   = {};
    };
    static_assert(std::is_trivially_copyable_v<PointLight>);
} // namespace anxiety::rendering::scene