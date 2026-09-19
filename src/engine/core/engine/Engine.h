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
	//   Engine engine({ .app_name = "MyGame", .target_fps = 60 });
	//   engine.emplace_module<InputModule>();			// depende de EventBus
	//   engine.emplace_module<EventBusModule>();		// registrado DESPUÉS de Input - pero se inicializa PRIMERO
	//
	//   engine.init();									// resuelve dependencias, ordena, inicializa en orden
	//   engine.run();									// bloquea; cada frame llama a on_update en orden ordenado
	//
	// Orden del ciclo de vida de los módulos:
	//   init     : ordenado topológicamente según las dependencias declaradas
	//   update   : mismo orden ordenado
	//   shutdown : orden inverso
	//
	// Unicidad: no se pueden registrar dos módulos con el mismo name().
	// --------------------------------------------------------------------------------------------
	class Engine {
	public:
		explicit Engine(EngineConfig config = {});
		~Engine();

		Engine(const Engine&)            = delete;
		Engine& operator=(const Engine&) = delete;

		// Ciclo de vida --------------------------------------------------------------------------
		// Resuelve el grafo de dependencias y luego inicializa los módulos en orden ordenado.
		// Devuelve false si el grafo no es válido (ciclo, dependencia ausente, nombre duplicado) o
		// si el on_init() de algún módulo devuelve false (los módulos ya iniciados se revierten
		// antes de devolver).
		[[nodiscard]] bool init();
		// Bloquea hasta que se llama a request_stop(), luego llama a shutdown() y regresa.
		void               run();
		// Llamado automáticamente por run() y por el destructor.
		void               shutdown();

		// Thread-safe. Se puede llamar desde cualquier hilo, incluso dentro de on_update().
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
		// Debe llamarse ANTES de init(). Rechaza:
		//   - módulos nulos.
		//   - módulos con el mismo nombre que uno ya registrado
		//   - llamadas realizadas después de init()
		void register_module(std::unique_ptr<IModule> module);

		// Accesos --------------------------------------------------------------------------------
		[[nodiscard]] const EngineConfig& config() const noexcept { return m_config; }
		[[nodiscard]] EngineState         state()  const noexcept { return m_state; }

	private:
		EngineConfig                          m_config;
		std::atomic<EngineState>              m_state         { EngineState::Uninitialized };
		std::atomic<bool>                     m_stop_requested{ false };
		// Contenedor propietario - direcciones estables, módulos vivos durante toda la vida del motor.
		std::vector<std::unique_ptr<IModule>> m_modules;
		// Vista no propietaria y ordenada topológicamente sobre m_modules.
		// Válida tras un init_modules() exitoso: se vacía al revertir o al apagar.
		std::vector<IModule*>                 m_sorted_order;

		// Resuelve dependencias, construye m_sorted_order, ejecuta on_init en orden con reversión.
		[[nodiscard]] bool init_modules();
		void               update_modules(float delta);
		void               shutdown_modules();
		void               tick(float delta);
	};
} // namespace anxiety