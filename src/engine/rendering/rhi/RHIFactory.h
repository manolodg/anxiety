#pragma once

#include "IDevice.h"
#include "RHICapabilities.h"

#include <memory>

namespace anxiety::rendering::rhi {
	// RHIFactory ---------------------------------------------------------------------------------
	// Crea instancias de IDevice para el backend apropiado.
	//
	// Orden de prioridad (selección automática):
	//   Win32:          DirectX12 --> DirectX11 --> Vulkan --> OpenGL
	//   Linux:          Vulkan --> OpenGL
	//   macOS:          Metal --> Vulkan --> OpenGL
	//   RPi:            Vulkan --> OpenGLES
	//   Android/Quest:  Vulkan --> OpenGLES
	//
	// Override por línea de comandos: "--rhi=dx12|dx11|vulkan|metal|opengle|gles"
	// --------------------------------------------------------------------------------------------
	class RHIFactory {
	public:
		// Analiza --rhi=<backend> desde argv. Devuelve Unknown si no está presente.
		[[nodiscard]] static RHIBackend parse_command_line(int argc, const char* const* argv)      noexcept;
		// Elige el mejor backend para la plataforma actual, con una preferencia opcional.
		[[nodiscard]] static RHIBackend select_backend(RHIBackend preferred = RHIBackend::Unknown) noexcept;

		// Crea un dispositivo para el backend dado. Devuelve nullptr si falla.
		// native_window: necesario para OpenGL en Win32 (HWND) y Linux (Window XID), de modo que
		// el backend pueda crear su contexto de renderizado antes de cargar los punteros a
		// función. Ignorado por DX12, DX11, Vulkan y Metal.
		[[nodiscard]] static std::unique_ptr<IDevice> create_device(RHIBackend backend, bool enable_validation = false, void* native_window = nullptr);

		// Conveniencia: analiza la línea de comandos + selecciona + crea en una sola llamada.
		[[nodiscard]] static std::unique_ptr<IDevice> create_best_device(int argc, const char* const* argv, bool enable_validation = false);
	};
} // namespace anxiety::rendering::rhi