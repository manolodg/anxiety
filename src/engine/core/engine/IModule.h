#pragma once

#include <vector>
#include <string_view>

namespace anxiety {
	class Engine;					// Declaración anticipada

	// IModule ------------------------------------------------------------------------------------
	// Implementa esta inferfaz para añadir un subsistema con nombre al motor.
	// 
	// Contrato del ciclo de vida:
	//   1. dependencies()	- se consulta una vez antes de init() para construir el grafo de
	//						  dependencias. Debe devolver los nombres de los módulos que tienen
	//						  que estar completamente inicializados antes que este nodo.
	//	 2. on_init(engine) - se llama en orden de registro durante Engine::init(). Se ha de devolver
	//                        false para abortar. Los módulos ya iniciados se apagan en orden inverso
	//                        antes de que init() devuelva false.
	//   3. on_update(dt)   - se llama cada frame dentro del bucle principal.
	//   4. on_shutdown()   - se llama en orden INVERSO al de registro durante Engine::shutdown().
	//
	// Nomenclatura:
	//   name() debe devolver un identificador no vacío y único en el proyecto. El motor obliga
	//   esta unicidad en el momento del registro.
	// --------------------------------------------------------------------------------------------
	class IModule {
	public:
		virtual ~IModule() = default;

		// Nombre estable y único para este módulo. Se usa para la búsqueda de dependencias.
		[[nodiscard]] virtual std::string_view              name()        const noexcept = 0;
		// Nombres de los módulos que deben inicializarse antes que este. La implementación por
		// defecto devuelve una lista vacía (sin dependencias).
		[[nodiscard]] virtual std::vector<std::string_view> dependencies() const { return {}; }

		// Devuelve false para indicar que este módulo no pudo iniciarse.
		[[nodiscard]] virtual bool on_init(Engine& engine) = 0;
		virtual void               on_update(float delta)  = 0;
		virtual void               on_shutdown()           = 0;

	protected:
		IModule() = default;

		IModule(const IModule&)            = delete;
		IModule& operator=(const IModule&) = delete;
	};
} // namespace anxiety