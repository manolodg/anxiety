#include "Engine.h"
#include "IModule.h"
#include "IPlatform.h"
#include "Logger.h"
#include "PlatformModule.h"

using namespace anxiety;

static constexpr std::string_view k_category = "sandbox";

// Módulos de ejemplo -----------------------------------------------------------------------------

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

	engine.emplace_module<AutoStopModule>(5.0);		// Se solicitará la parada pasados 5 segundos
	engine.emplace_module<ClockModule>();

	platform::PlatformModule::Config pal_cfg;
	pal_cfg.window.title     = "Anxiety - Ejemplo en Ventana";
	pal_cfg.window.width     = 800;
	pal_cfg.window.height    = 600;
	pal_cfg.window.resizable = true;
	engine.emplace_module<platform::PlatformModule>(pal_cfg);

	if (!engine.init()) { LOG_FATAL(k_category, "El motor no ha podido iniciarse."); return 1; }

	engine.run();

	return 0;
}
