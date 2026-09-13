#pragma once

#include "IWindow.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace anxiety::platform {
	class PlatformModule;							// gestiona el ciclo de vida del singleton

	// IPlatform ----------------------------------------------------------------------------------
	// Fábrica abstracta de todos los subsistemas específicos de la plataforma.
	//
	// Una implementación concreta (p. ej. Win32Platform) es creada y registrada por PlatformModule
	// durante on_init(). A partir de ahí, se puede llamar a IPlatform::current() desde cualquier
	// parte del motor para acceder a la plataforma activa.
	//
	// Ciclo de vida:
	//   Válida desde PlatformModule::on_init() hasta PlatformModule::on_shutdown(). Llamar a
	//   current() fuera de esa ventana es comportamiento indefinido.
	//
	// Thread safety:
	//   current(), sleep_ms() - thread-safe.
	//   create_window()       - debe llamarse desde el hilo principal.
	// --------------------------------------------------------------------------------------------
	class IPlatform {
	public:
		virtual ~IPlatform()                   = default;
		IPlatform(const IPlatform&)            = delete;
		IPlatform& operator=(const IPlatform&) = delete;

		// Identificador de la plataforma para el log ("Win32", "Linux", ...).
		[[nodiscard]] virtual std::string_view name() const noexcept = 0;

		// Métodos de fábrica ----------------------------------------------------------------------
		[[nodiscard]] virtual std::unique_ptr<IWindow> create_window(const IWindow::Desc& desc) = 0;

		// Suspende el hilo llamante durante al menos 'milliseconds' ms.
		virtual void sleep_ms(uint32_t milliseconds) noexcept = 0;

		// Singleton gestionado por el módulo -------------------------------------------------------
		// Devuelve la plataforma activa. Solo es válido tras PlatformModule::on_init().
		[[nodiscard]] static IPlatform& current();

	protected:
		IPlatform() = default;
	private:
		// Solo PlatformModule puede instalar / retirar el singleton.
		friend class PlatformModule;
		static void set_current(IPlatform* p) noexcept;
	};
} // namespace anxiety::platform