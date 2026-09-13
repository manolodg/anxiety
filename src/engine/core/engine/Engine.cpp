#include "Engine.h"
#include "Logger.h"

#include <chrono>
#include <format>

namespace anxiety {
	static constexpr std::string_view k_category = "Engine";

	// Constructor / Destructor -------------------------------------------------------------------
	Engine::Engine(EngineConfig config) : m_config(std::move(config)) {}
	Engine::~Engine() { if (m_state != EngineState::Stopped && m_state != EngineState::Uninitialized) shutdown(); }

	// Ciclo de vida ------------------------------------------------------------------------------
	bool Engine::init() { 
		if (m_state != EngineState::Uninitialized) {
			LOG_WARNING(k_category, "init() llamado sobre un Engine ya inicializado - ignorado.");
			return false;
		}

		m_state = EngineState::Initializing;
		LOGF_INFO(k_category, "Inicializando '{}' ({} módulo(s))", m_config.app_name, m_modules.size());

		if (!init_modules()) {
			LOG_ERROR(k_category, "La inicialización de módulos falló - el motor no arrancará.");
			m_state = EngineState::Uninitialized;
			return false;
		}

		m_state = EngineState::Running;
		LOG_INFO(k_category, "Engine inicializado correctamente.");

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

		LOGF_INFO(k_category, "Registrando módulo '{}'.", module->name());
		m_modules.push_back(std::move(module));
	}

	bool Engine::init_modules() {
		for (size_t i = 0; i < m_modules.size(); ++i) {
			LOGF_INFO(k_category, "Inicializando módulo '{}'.", m_modules[i]->name());

			if (!m_modules[i]->on_init(*this)) {
				LOGF_ERROR(k_category, "El módulo '{}' falló al inicializarse - revirtiendo.", m_modules[i]->name());

				// Apaga los módulos que ya se habían inicializado correctamente, en orden inverso.
				for (size_t j = i; j-- > 0;) {
					LOGF_INFO(k_category, "Revirtiendo módulo '{}'.", m_modules[j]->name());
					m_modules[j]->on_shutdown();
				}

				return false;
			}
		}

		return true;
	}

	void Engine::update_modules(float delta) {
		for (auto& mod : m_modules) {
			mod->on_update(delta);
		}
	}

	void Engine::shutdown_modules() {
		// Orden inverso al de registro - último en entrar, primero en salir.
		for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
			LOGF_INFO(k_category, "Apagando módulo '{}'.", (*it)->name());
			(*it)->on_shutdown();
		}
	}

	// Tick por frame -----------------------------------------------------------------------------
	void Engine::tick(float delta) {
		update_modules(delta);
	}
} // namespace anxiety