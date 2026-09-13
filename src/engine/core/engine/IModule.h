#pragma once

#include <string_view>

namespace anxiety {
	class Engine;					// Declaración anticipada

	// IModule ------------------------------------------------------------------------------------
	// Implementa esta inferfaz para añadir un subsistema con nombre al motor.
	// 
	// Contrato del ciclo de vida:
	//	1. on_init(engine) - se llama en orden de registro durante Engine::init(). Se ha de devolver
	//                       false para abortar. Los módulos ya iniciados se apagan en orden inverso
	//                       antes de que init() devuelva false.
	//  2. on_update(dt)   - se llama cada frame dentro del bucle principal.
	//  3. on_shutdown()   - se llama en orden INVERSO al de registro durante Engine::shutdown().
	// --------------------------------------------------------------------------------------------
	class IModule {
	public:
		virtual ~IModule() = default;

		[[nodiscard]] virtual std::string_view name()                  const noexcept = 0;
		// Devuelve false para indicar que este módulo no pudo iniciarse.
		[[nodiscard]] virtual bool             on_init(Engine& engine)                = 0;

		virtual void on_update(float delta) = 0;
		virtual void on_shutdown()          = 0;

	protected:
		IModule() = default;

		IModule(const IModule&)            = delete;
		IModule& operator=(const IModule&) = delete;
	};
} // namespace anxiety