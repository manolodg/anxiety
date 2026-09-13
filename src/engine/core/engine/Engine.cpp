#include "Engine.h"
#include "Logger.h"

#include <chrono>

namespace anxiety {
	static constexpr std::string_view k_category = "Engine";

	// Constructor / Destructor -------------------------------------------------------------------
	Engine::Engine(EngineConfig config) : m_config(std::move(config)) {}
	Engine::~Engine() { if (m_state != EngineState::Stopped && m_state != EngineState::Uninitialized) shutdown(); }

	// Ciclo de vida ------------------------------------------------------------------------------
	bool Engine::init() { 
		m_state = EngineState::Initializing;
		LOGF_INFO(k_category, "Iniciando el motor: {}", m_config.app_name);

		// La inicialización de plataforma / subsistemas se registraría aquí mediante un
		// SystemRegistry. Para MP-00 solo validamos el flujo del ciclo de vida.

		LOG_INFO(k_category, "Motor inicializado correctamente.");
		m_state = EngineState::Running;

		return true;
	}

	void Engine::run() {
		if (m_state != EngineState::Running) { LOG_ERROR(k_category, "run() require que el motor esté en estado Running. Llama antes a init()."); return; }

		using Clock   = std::chrono::steady_clock;
		using Seconds = std::chrono::duration<float>;

		LOGF_INFO(k_category, "Entrando en el bucle principal (fps objetivo: {})", m_config.target_fps);

		auto last_time = Clock::now();

		while (!m_stop_requested) {
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

		m_state = EngineState::Stopped;
	}

	void Engine::request_stop() noexcept {
		LOG_INFO(k_category, "Parada solicitada.");
		m_stop_requested = true;
	}

	// Tick por frame -----------------------------------------------------------------------------
	void Engine::tick(float /*delta*/) {
	}
} // namespace anxiety