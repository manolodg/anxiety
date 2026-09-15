#pragma once

#include <cstdint>

namespace anxiety::rendering::rhi {
	enum class RHIBackend : uint8_t {
		Unknown,
		DirectX12,
		DirectX11,
		Vulkan,
		Metal,
		OpenGL,
		OpenGLES
	};

	// Devuelve una cadena legible para mostrar de un valor de backend.
	inline const char* backend_name(RHIBackend backend) noexcept {
		switch (backend) {
		case RHIBackend::DirectX12: return "DirectX 12";
		case RHIBackend::DirectX11: return "DirectX 11";
		case RHIBackend::Vulkan:    return "Vulkan";
		case RHIBackend::Metal:     return "Metal";
		case RHIBackend::OpenGL:    return "OpenGL";
		case RHIBackend::OpenGLES:  return "OpenGL ES";
		default:                    return "Unknown";
		}
	}
} // namespace anxiety::rendering::rhi