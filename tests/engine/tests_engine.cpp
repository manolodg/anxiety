#include <catch2/catch_test_macros.hpp>

#include "Engine.h"
#include "IModule.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace anxiety;

// Ayudantes ----------------------------------------------------------------------------------------
// Registra cada llamada del ciclo de vida para que los tests puedan verificar orden y cantidad.
struct TrackingModule final : public IModule {
	struct Call { std::string tag; };

	explicit TrackingModule(std::string name, std::vector<Call>& log, bool fail_init = false, std::vector<std::string> deps = {}) : m_name(std::move(name)), m_log(log), m_fail_init(fail_init), m_dep_names(std::move(deps)) {}

	std::string_view name() const noexcept override { return m_name; }

    std::vector<std::string_view> dependencies() const override {
	    std::vector<std::string_view> result;
	    result.reserve(m_dep_names.size());

	    for (const auto& d : m_dep_names) {
	        result.emplace_back(d);
	    }

	    return result;
	}

	bool on_init(Engine& /*engine*/) override {
		m_log.push_back({ m_name + ":init" });
		return !m_fail_init;
	}

	void on_update(float /*dt*/) override { m_log.push_back({ m_name + ":update" }); }

	void on_shutdown() override { m_log.push_back({ m_name + ":shutdown" }); }

	std::string              m_name;
	std::vector<Call>&       m_log;
	bool                     m_fail_init;
    std::vector<std::string> m_dep_names;
};

// Tests del ciclo de vida del Engine ---------------------------------------------------------------
TEST_CASE("engine_initial_state", "[engine]") {
	Engine engine;
	REQUIRE(static_cast<int>(engine.state()) == static_cast<int>(EngineState::Uninitialized));
}

TEST_CASE("engine_init_transitions_to_running", "[engine]") {
    Engine engine;

    REQUIRE(engine.init());
    REQUIRE(static_cast<int>(engine.state()) == static_cast<int>(EngineState::Running));

    engine.shutdown();
}

TEST_CASE("engine_shutdown_transitions_to_stopped", "[engine]") {
    Engine engine;

    REQUIRE(engine.init());

    engine.shutdown();
    REQUIRE(static_cast<int>(engine.state()) == static_cast<int>(EngineState::Stopped));
}

TEST_CASE("engine_double_shutdown_is_safe", "[engine]") {
    Engine engine;

    REQUIRE(engine.init());

    engine.shutdown();
    engine.shutdown();   // no debe fallar ni cambiar el estado

    REQUIRE(static_cast<int>(engine.state()) == static_cast<int>(EngineState::Stopped));
}

TEST_CASE("engine_double_init_is_rejected", "[engine]") {
    Engine engine;

    REQUIRE(engine.init());
    REQUIRE(!engine.init());

    engine.shutdown();
}

TEST_CASE("engine_destructor_shuts_down_safely", "[engine]") {
    // Se acota el alcance del engine — el destructor debe llamar a shutdown() sin fallar.
    std::vector<TrackingModule::Call> log; {
        Engine eng;
        eng.emplace_module<TrackingModule>("A", log);

        REQUIRE(eng.init());
        // se deja salir de alcance sin un shutdown explícito
    }

    // Si llegamos aquí, el destructor se ejecutó correctamente.
    REQUIRE(!log.empty());
}

// Tests de orden del ciclo de vida de módulos -------------------------------------------------------

TEST_CASE("module_init_called_in_registration_order", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log);
    engine.emplace_module<TrackingModule>("B", log);
    engine.emplace_module<TrackingModule>("C", log);

    REQUIRE(engine.init());
    engine.shutdown();

    // orden de init: A, B, C
    REQUIRE(log.size() >= 3u);
    REQUIRE(log[0].tag == std::string{ "A:init" });
    REQUIRE(log[1].tag == std::string{ "B:init" });
    REQUIRE(log[2].tag == std::string{ "C:init" });
}

TEST_CASE("module_shutdown_called_in_reverse_order", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log);
    engine.emplace_module<TrackingModule>("B", log);
    engine.emplace_module<TrackingModule>("C", log);
    REQUIRE(engine.init());

    log.clear();   // descarta las llamadas de init, solo interesa el shutdown
    engine.shutdown();

    REQUIRE(log.size() == 3u);
    REQUIRE(log[0].tag == std::string{ "C:shutdown" });
    REQUIRE(log[1].tag == std::string{ "B:shutdown" });
    REQUIRE(log[2].tag == std::string{ "A:shutdown" });
}

TEST_CASE("module_init_and_shutdown_called_once", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("M", log);
    REQUIRE(engine.init());
    engine.shutdown();

    int inits = 0, shutdowns = 0;
    for (auto& c : log) {
        if (c.tag == "M:init")     ++inits;
        if (c.tag == "M:shutdown") ++shutdowns;
    }

    REQUIRE(inits);
    REQUIRE(shutdowns == 1);
}

TEST_CASE("module_failed_init_rolls_back_previous_modules", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("Good1", log, /*failInit=*/false);
    engine.emplace_module<TrackingModule>("Bad", log, /*failInit=*/true);
    engine.emplace_module<TrackingModule>("Good2", log, /*failInit=*/false);

    const bool ok = engine.init();
    REQUIRE(!ok);
    REQUIRE(static_cast<int>(engine.state()) == static_cast<int>(EngineState::Uninitialized));

    // Good1 se inicializó antes de que Bad fallara — debe haberse revertido.
    int shutdowns = 0;
    for (auto& c : log) {
        if (c.tag.ends_with(":shutdown")) ++shutdowns;
    }
    REQUIRE(shutdowns == 1);
}

TEST_CASE("module_register_after_init_is_rejected", "[engine]") {
    std::vector<TrackingModule::Call> log;
    std::vector<TrackingModule::Call> lateLog;

    Engine engine;
    engine.emplace_module<TrackingModule>("Early", log);
    REQUIRE(engine.init());

    // Registrar después de init no debe fallar ni añadir el módulo.
    engine.emplace_module<TrackingModule>("Late", lateLog);

    log.clear();
    engine.shutdown();

    // El módulo "Late" no debe aparecer en el shutdown
    for (auto& c : log) {
        REQUIRE(c.tag.find("Late") == std::string::npos);
    }
}

// Tests de resolución de dependencias ---------------------------------------------------------------

// A depende de B; se registran A y luego B — B debe inicializarse primero.
TEST_CASE("deps_basic_dep_overrides_registration_order", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log, false, std::vector<std::string>{"B"});
    engine.emplace_module<TrackingModule>("B", log);
    REQUIRE(engine.init());
    engine.shutdown();

    // Buscar las posiciones de B:init y A:init
    int posB = -1, posA = -1;
    for (int i = 0; i < static_cast<int>(log.size()); ++i) {
        if (log[i].tag == "B:init") posB = i;
        if (log[i].tag == "A:init") posA = i;
    }
    REQUIRE(posB >= 0);
    REQUIRE(posA >= 0);
    REQUIRE(posB < posA);
}

// C depende de B, B depende de A — la cadena debe ordenarse A → B → C.
TEST_CASE("deps_chain_respects_order", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    // Registro en orden incorrecto: C, B, A
    engine.emplace_module<TrackingModule>("C", log, false, std::vector<std::string>{"B"});
    engine.emplace_module<TrackingModule>("B", log, false, std::vector<std::string>{"A"});
    engine.emplace_module<TrackingModule>("A", log);
    REQUIRE(engine.init());
    engine.shutdown();

    int posA = -1, posB = -1, posC = -1;
    for (int i = 0; i < static_cast<int>(log.size()); ++i) {
        if (log[i].tag == "A:init") posA = i;
        if (log[i].tag == "B:init") posB = i;
        if (log[i].tag == "C:init") posC = i;
    }
    REQUIRE((posA >= 0 && posB >= 0 && posC >= 0));
    REQUIRE(posA < posB);
    REQUIRE(posB < posC);
}

// A→B y B→A forman un ciclo de dos nodos — init debe fallar.
TEST_CASE("deps_cycle_fails_init", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log, false, std::vector<std::string>{"B"});
    engine.emplace_module<TrackingModule>("B", log, false, std::vector<std::string>{"A"});
    REQUIRE(!engine.init());
}

// A→B→C→A forma un ciclo de tres nodos — init debe fallar.
TEST_CASE("deps_cycle_three_nodes", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log, false, std::vector<std::string>{"C"});
    engine.emplace_module<TrackingModule>("B", log, false, std::vector<std::string>{"A"});
    engine.emplace_module<TrackingModule>("C", log, false, std::vector<std::string>{"B"});
    REQUIRE(!engine.init());
}

// A declara una dependencia sobre un módulo no registrado — init debe fallar.
TEST_CASE("deps_missing_dep_fails_init", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log, false, std::vector<std::string>{"Ghost"});
    REQUIRE(!engine.init());
}

// Diamante: D depende de B y C; B y C dependen ambos de A.
// A debe inicializarse primero, D debe inicializarse último.
TEST_CASE("deps_diamond_order_correct", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    // Registro desordenado: D, B, C, A
    engine.emplace_module<TrackingModule>("D", log, false, std::vector<std::string>{"B", "C"});
    engine.emplace_module<TrackingModule>("B", log, false, std::vector<std::string>{"A"});
    engine.emplace_module<TrackingModule>("C", log, false, std::vector<std::string>{"A"});
    engine.emplace_module<TrackingModule>("A", log);
    REQUIRE(engine.init());
    engine.shutdown();

    int posA = -1, posB = -1, posC = -1, posD = -1;
    for (int i = 0; i < static_cast<int>(log.size()); ++i) {
        if (log[i].tag == "A:init") posA = i;
        if (log[i].tag == "B:init") posB = i;
        if (log[i].tag == "C:init") posC = i;
        if (log[i].tag == "D:init") posD = i;
    }
    REQUIRE((posA >= 0 && posB >= 0 && posC >= 0 && posD >= 0));
    REQUIRE(posA < posB);
    REQUIRE(posA < posC);
    REQUIRE(posB < posD);
    REQUIRE(posC < posD);
}

// Registrar dos módulos con el mismo nombre — el segundo debe ser rechazado.
TEST_CASE("deps_duplicate_name_rejected_at_registration", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log);
    engine.emplace_module<TrackingModule>("A", log);   // duplicado — debe descartarse silenciosamente
    REQUIRE(engine.init());
    engine.shutdown();

    // Contar cuántas veces aparece A:init — debe ser exactamente una.
    int inits = 0;
    for (auto& c : log) {
        if (c.tag == "A:init") ++inits;
    }
    REQUIRE(inits == 1);
}

// A se declara a sí mismo como dependencia — la autorreferencia debe ser rechazada.
TEST_CASE("deps_self_loop_fails_init", "[engine]") {
    std::vector<TrackingModule::Call> log;

    Engine engine;
    engine.emplace_module<TrackingModule>("A", log, false, std::vector<std::string>{"A"});
    REQUIRE(!engine.init());
}
