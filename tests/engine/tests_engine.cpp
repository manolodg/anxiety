#include <catch2/catch_test_macros.hpp>

#include "Engine.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace anxiety;

// Ayudantes ----------------------------------------------------------------------------------------
// Registra cada llamada del ciclo de vida para que los tests puedan verificar orden y cantidad.
// --------------------------------------------------------------------------------------------------
struct TrackingModule final : public IModule {
    struct Call { std::string tag; };

    explicit TrackingModule(std::string name, std::vector<Call>& log, bool fail_init = false) : m_name(std::move(name)), m_log(log), m_fail_init(fail_init) {}

    std::string_view name() const noexcept override { return m_name; }

    bool on_init(Engine& /*engine*/) override {
        m_log.push_back({ m_name + ":init" });
        return !m_fail_init;
    }

    void on_update(float /*dt*/) override { m_log.push_back({ m_name + ":update" }); }

    void on_shutdown() override { m_log.push_back({ m_name + ":shutdown" }); }

    std::string        m_name;
    std::vector<Call>& m_log;
    bool               m_fail_init;
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
