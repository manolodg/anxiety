#include "Engine.h"
#include "IModule.h"
#include "IPlatform.h"
#include "Logger.h"
#include "PlatformModule.h"
#include "RenderingModule.h"

using namespace anxiety;

static constexpr std::string_view k_category = "sandbox";

// Módulos de ejemplo -----------------------------------------------------------------------------

// Demostración de cadena de dependencias ---------------------------------------------------------
//
// Grafo de dependencias declarado:
//
//   AutoStop ←── Platform ←── Clock ←── EventBus ←── Input ──────┐
//                    └────── Physics ─────┼──► Render
//                                         │
//                                      AutoStop (detiene el motor tras N fotogramas)
//
// Orden de registro (deliberadamente desordenado):
//   [0] Render, [1] Physics, [2] AutoStop, [3] Input, [4] EventBus, [5] Platform
//
// Orden de inicialización resuelto esperado:
//   AutoStop -> Platform -> Clock -> EventBus -> Rendering -> Physics -> Input -> Render
//	 (Platform no tiene deps -> se ordena primero; AutoStop no tiene deps -> se ordena último)
// ------------------------------------------------------------------------------------------------

// EventBusModule ---------------------------------------------------------------------------------
class EventBusModule final : public IModule {
public:
	std::string_view name() const noexcept override { return "EventBus"; }

	std::vector<std::string_view> dependencies() const override { return { "Platform" }; }

	bool on_init(Engine& /*engine*/) override {
		LOG_INFO("EventBus", "EventBusModule en línea — enrutamiento de eventos listo.");
		return true;
	}

	void on_update(float /*dt*/) override {}
	void on_shutdown() override { LOG_INFO("EventBus", "EventBusModule desconectado."); }
};

// InputModule ------------------------------------------------------------------------------------
class InputModule final : public IModule {
public:
	std::string_view name() const noexcept override { return "Input"; }

	std::vector<std::string_view> dependencies() const override { return { "EventBus" }; }

	bool on_init(Engine& /*engine*/) override {
		LOG_INFO("Input", "InputModule en línea — consultando dispositivos.");
		return true;
	}

	void on_update(float /*dt*/) override {}
	void on_shutdown() override { LOG_INFO("Input", "InputModule desconectado."); }
};

// PhysicsModule ----------------------------------------------------------------------------------
class PhysicsModule final : public IModule {
public:
	std::string_view name() const noexcept override { return "Physics"; }

	std::vector<std::string_view> dependencies() const override { return { "EventBus" }; }

	bool on_init(Engine& /*engine*/) override {
		LOG_INFO("Physics", "PhysicsModule en línea — simulación lista.");
		return true;
	}

	void on_update(float /*dt*/) override {}
	void on_shutdown() override { LOG_INFO("Physics", "PhysicsModule desconectado."); }
};

// RenderModule -----------------------------------------------------------------------------------
class RenderModule final : public IModule {
public:
	std::string_view name() const noexcept override { return "Render"; }

	std::vector<std::string_view> dependencies() const override { return { "Input", "Physics" }; }

	bool on_init(Engine& /*engine*/) override {
		LOG_INFO("Render", "RenderModule en línea — salida de frame lista.");
		return true;
	}

	void on_update(float /*dt*/) override {}
	void on_shutdown() override { LOG_INFO("Render", "RenderModule desconectado."); }
};


// AutoStopModule ---------------------------------------------------------------------------------
// Solicita el apagado del motor tras 'max_seconds' segundos. Demuestra el concepto de un módulo.
// ------------------------------------------------------------------------------------------------
class AutoStopModule final : public IModule {
public:
	explicit AutoStopModule(double max_seconds) : m_max_seconds(max_seconds) {}

	std::string_view name() const noexcept override { return "AutoStop"; }

	bool on_init(Engine& engine) override {
		LOG_INFO(k_category, "iniciandose...");
		LOGF_INFO(k_category, "Se solicitará la parada tras {} segundos.", m_max_seconds);

		m_engine = &engine;
		return true;
	}

	void on_update(float dt) override {
		m_elapsed += static_cast<double>(dt);

		if (m_elapsed >= m_max_seconds) {
			LOGF_INFO(k_category, "Se alcanzaron {} segundos - solicitando parada.", m_max_seconds);
			m_engine->request_stop();
		}
	}

	void on_shutdown() override { LOG_INFO(k_category, "apagado..."); }

private:
	static constexpr std::string_view k_category = "AutoStop";

	double  m_max_seconds{ 5.0 };
	Engine* m_engine{ nullptr };
	double  m_elapsed{ 0 };
};

// ClockModule ------------------------------------------------------------------------------------
// Lanza un mensaje en pantalla cada segúndo indicando el tiempo que ha transcurrido.
// ------------------------------------------------------------------------------------------------
class ClockModule final : public IModule {
public:
	std::string_view name() const noexcept override { return "Clock"; }

	std::vector<std::string_view> dependencies() const override { return { "AutoStop" }; }

	bool on_init(Engine& engine) override {
		LOG_INFO(k_category, "iniciandose...");

		LOGF_INFO(k_category, "Nombre de la app : '{}'", engine.config().app_name);
		LOGF_INFO(k_category, "Fps objetivo     : {}", engine.config().target_fps);

		return true;
	}

	void on_update(float delta) override {
		m_accum_delta += static_cast<double>(delta);

		int actual_second = static_cast<int>(m_accum_delta);
		if (m_actual_second < actual_second) LOGF_INFO(k_category, "Estamos en el segundo {}", actual_second);

		m_actual_second = actual_second;
	}

	void on_shutdown() override { LOGF_INFO(k_category, "apagado..."); }

private:
	static constexpr std::string_view k_category = "Clock";

	double m_accum_delta  { 0.0 };
	int    m_actual_second{ 0 };
};

// main ===========================================================================================
int main() {
	Engine engine;

	platform::PlatformModule::Config pal_cfg;
	pal_cfg.window.title     = "Anxiety - Ejemplo en Ventana";
	pal_cfg.window.width     = 800;
	pal_cfg.window.height    = 600;
	pal_cfg.window.resizable = true;

	// Orden de registro (desordenado — la ordenación por dependencias lo corregirá):
	//   Render, Physics, Clock, AutoStop, Input, EventBus, Platform, Rendering
	engine.emplace_module<RenderModule>();
	engine.emplace_module<PhysicsModule>();
	engine.emplace_module<ClockModule>();
	engine.emplace_module<AutoStopModule>(5.0);		// Se solicitará la parada pasados 5 segundos
	engine.emplace_module<InputModule>();
	engine.emplace_module<EventBusModule>();
	auto& plat_mod = engine.emplace_module<platform::PlatformModule>(pal_cfg);
	engine.emplace_module<rendering::RenderingModule>(plat_mod);

	if (!engine.init()) { LOG_FATAL(k_category, "El motor no ha podido iniciarse."); return 1; }

	engine.run();

	return 0;
}
