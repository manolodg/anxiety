#pragma once

#include "IModule.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace anxiety {
    // EngineConfig - configuración del motor ------------------------------------------------------
    struct EngineConfig {
        std::string app_name     = "Anxiety App";
        uint32_t    target_fps   = 60;
        bool        headless     = false;			// sin ventana / sin salida de render
    };

    // EngineState - estados del ciclo de vida del motor -------------------------------------------
    enum class EngineState : uint8_t {
        Uninitialized,
        Initializing,
        Running,
        Paused,
        ShuttingDown,
        Stopped
    };

	// Engine -------------------------------------------------------------------------------------
	// Objeto central del runtime. Uso típico:
	// 
	//   Engine engine();
	//   engine.init();
	//   engine.run();					// bloquea hasta que se llama a request_stop()
	//									// shutdown() se llama implícitamente al final de run()
	//
	// Ciclo de vida de los módulos (por frame):
	//   orden de init    : orden de registro
	//   orden de update  : orden de registro
	//   shutdown         : orden inverso al de registro
	// --------------------------------------------------------------------------------------------
	class Engine {
	public:
		explicit Engine(EngineConfig config = {});
		~Engine();

		Engine(const Engine&)            = delete;
		Engine& operator=(const Engine&) = delete;

		// Ciclo de vida --------------------------------------------------------------------------
		// init() debe llamarse antes de run().
		// Devuelve false si algún módulo falla al inicializarse (los módulos ya iniciados se
		// apagan antes de devolver el resultado).
		[[nodiscard]] bool init();
		// Bloquea hasta que se llama a request_stop(), luego llama a shutdown() y regresa.
		void               run();
		// Llamado automáticamente por run() y por el destructor.
		void               shutdown();

		// Se solicita la parada del game loop.
		void               request_stop() noexcept;

		// Registro de módulos --------------------------------------------------------------------
		// Construye un módulo in-place y lo registra. T debe derivar de IModule.
		// Devuelve una referencia estable durante toda la vida del Engine.
		template<typename T, typename... Args>
		T& emplace_module(Args&&... args) {
			static_assert(std::is_base_of_v<IModule, T>, "T ha de derivar de anxiety::IModule");

			auto ptr = std::make_unique<T>(static_cast<Args&&>(args)...);
			T& ref = *ptr;
			register_module(std::move(ptr));

			return ref;
		}
		// Debe llamarse ANTES de init(). Llamarlo después de init() rechaza el módulo.
		void register_module(std::unique_ptr<IModule> module);

		// Accesos --------------------------------------------------------------------------------
		[[nodiscard]] const EngineConfig& config() const noexcept { return m_config; }
		[[nodiscard]] EngineState         state()  const noexcept { return m_state; }

	private:
		EngineConfig                          m_config;
		EngineState                           m_state         { EngineState::Uninitialized };
		std::atomic<bool>                     m_stop_requested{ false };
		std::vector<std::unique_ptr<IModule>> m_modules;

		[[nodiscard]] bool init_modules();
		void               update_modules(float delta);
		void               shutdown_modules();
		void               tick(float delta);
	};
} // namespace anxiety