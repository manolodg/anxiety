#include "Engine.h"
#include "Logger.h"
#include "ModuleGraph.h"

#include <chrono>
#include <thread>

namespace anxiety {
	static constexpr std::string_view k_category = "Engine";

	// Constructor / Destructor -------------------------------------------------------------------
	Engine::Engine(EngineConfig config) : m_config(std::move(config)) {}
	Engine::~Engine() { if (m_state != EngineState::Stopped && m_state != EngineState::Uninitialized) shutdown(); }

	// Ciclo de vida ------------------------------------------------------------------------------
	bool Engine::init() { 
		if (m_state != EngineState::Uninitialized) {
			LOG_WARNING(k_category, "init() llamado sobre un Engine ya iniciado - ignorado.");
			return false;
		}

		m_state = EngineState::Initializing;
		LOGF_INFO(k_category, "Iniciado '{}' ({} módulo(s))", m_config.app_name, m_modules.size());

		if (!init_modules()) {
			LOG_ERROR(k_category, "El iniciado de módulos falló - el motor no arrancará.");
			m_state = EngineState::Uninitialized;
			return false;
		}

		m_state = EngineState::Running;
		LOG_INFO(k_category, "Engine iniciado correctamente.");

		return true;
	}

	void Engine::run() {
		if (m_state != EngineState::Running) { LOG_ERROR(k_category, "run() require que el motor esté en estado Running. Llama antes a init()."); return; }

		using Clock   = std::chrono::steady_clock;
		using Seconds = std::chrono::duration<float>;

		LOGF_INFO(k_category, "Entrando en el bucle principal (fps objetivo: {})", m_config.target_fps);

		auto last_time = Clock::now();

		while (!m_stop_requested.load(std::memory_order_relaxed)) {
			const auto  now = Clock::now();
			const float dt  = Seconds(now - last_time).count();
			last_time = now;

			tick(dt);
		}

		LOG_INFO(k_category, "Bucle principal finalizado.");

		shutdown();
	}

	void Engine::shutdown() {
		if (m_state == EngineState::Stopped || m_state == EngineState::Uninitialized) return;

		m_state = EngineState::ShuttingDown;
		LOG_INFO(k_category, "Motor apagadondose...");

		shutdown_modules();

		m_sorted_order.clear();

		m_state = EngineState::Stopped;
		LOG_INFO(k_category, "Engine detenido.");
	}

	void Engine::request_stop() noexcept {
		LOG_INFO(k_category, "Parada solicitada.");
		m_stop_requested.store(true, std::memory_order_relaxed);
	}

	// Gestión de módulos ---------------------------------------------------------------------------
	void Engine::register_module(std::unique_ptr<IModule> module) {
		if (!module) {
			LOG_WARNING(k_category, "register_module() llamado con un módulo nulo - ignorado.");
			return;
		}
		if (m_state != EngineState::Uninitialized) {
			LOGF_ERROR(k_category, "register_module('{}') debe llamarse antes de init().", module->name());
			return;
		}

		// Impone unicidad de nombre - O(n) sobre un conjunto que siempre es pequeño.
		for (const auto& existing : m_modules) {
			if (existing->name() == module->name()) {
				LOGF_ERROR(k_category, "Ya hay un módulo registrado con el nombre '{}' - rechazado.", module->name());
				return;
			}
		}

		LOGF_INFO(k_category, "Registrando módulo '{}'.", module->name());
		m_modules.push_back(std::move(module));
	}

	// init_modules - resolución de dependencias + inicialización ordenada ------------------------
	bool Engine::init_modules() {
		m_sorted_order.clear();

		// Resolver el grafo de dependencias -------------------------------------------------------
		auto result = ModuleGraph::resolve(m_modules);
		if (!result.success) {
			LOGF_ERROR(k_category, "Falló la resolución de dependencias: {}", result.error);
			return false;
		}

		m_sorted_order = std::move(result.order);

		// Registrar en el log el orden resuelto ---------------------------------------------------
		{
			std::string order;
			order.reserve(m_sorted_order.size() * 12);
			for (size_t i = 0; i < m_sorted_order.size(); ++i) {
				if (i > 0) order += " -> ";
				order += m_sorted_order[i]->name();
			}

			LOGF_INFO(k_category, "Orden de inicialización resuelto: [{}]", order);
		}

		// Inicializar en el orden ordenado --------------------------------------------------------
		for (size_t i = 0; i < m_sorted_order.size(); ++i) {
			IModule* mod = m_sorted_order[i];
			LOGF_INFO(k_category, "Inicializando módulo '{}'.", mod->name());

			if (!mod->on_init(*this)) {
				LOGF_ERROR(k_category, "El módulo '{}' no pudo inicializarse - revirtiendo.", mod->name());

				// Desmonta los módulos que ya se habían inicializado, en orden inverso.
				for (size_t j = i; j-- > 0;) {
					LOGF_INFO(k_category, "Revirtiendo módulo '{}'.", m_sorted_order[j]->name());
					m_sorted_order[j]->on_shutdown();
				}

				m_sorted_order.clear();
				return false;
			}
		}

		return true;
	}

	// update_modules / shutdown_modules ----------------------------------------------------------
	void Engine::update_modules(float delta) {
		for (IModule* mod : m_sorted_order) {
			mod->on_update(delta);
		}
	}

	void Engine::shutdown_modules() {
		for (auto it = m_sorted_order.rbegin(); it != m_sorted_order.rend(); ++it) {
			LOGF_INFO(k_category, "Apagando módulo '{}'.", (*it)->name());
			(*it)->on_shutdown();
		}
	}

	// Tick por frame -----------------------------------------------------------------------------
	void Engine::tick(float delta) {
		update_modules(delta);
	}
} // namespace anxiety