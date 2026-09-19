#pragma once

#include "rhi/RHITypes.h"

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
        rhi::BufferHandle vertex_buffer;            // datos de vértice intercalados
        rhi::BufferHandle index_buffer;             // datos de índice (uint32_t)
        uint32_t          index_count   = 0;
        uint32_t          vertex_stride = 28;       // bytes por vértice (pos+col por defecto)
        uint32_t          material_id   = 0;        // reservado para un futuro sistema de materiales
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
} // namespace anxiety::rendering::scene