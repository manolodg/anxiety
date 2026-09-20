#include <catch2/catch_test_macros.hpp>

#include "Engine.h"
#include "PlatformModule.h"
#include "IPlatform.h"

#include <atomic>
#include <cstdint>

using namespace anxiety::platform;

// Ayudantes ----------------------------------------------------------------------------------------

// Construye un Engine headless con PlatformModule registrado e inicializado. Quien llama recibe la
// referencia PlatformModule& para inspección.
struct HeadlessPlatformFixture {
    anxiety::Engine engine;
    PlatformModule&       mod;

    HeadlessPlatformFixture() : engine(anxiety::EngineConfig{ .headless = true }) , mod(engine.emplace_module<PlatformModule>()) {
        (void)engine.init();                        // el valor de retorno se verifica por test vía engine.state()
    }
    ~HeadlessPlatformFixture() { engine.shutdown(); }
};

// Ciclo de vida de PlatformModule -------------------------------------------------------------------

TEST_CASE("platform_module_headless_init_succeeds", "[platform]") {
    HeadlessPlatformFixture f;
    REQUIRE(static_cast<int>(f.engine.state()) == static_cast<int>(anxiety::EngineState::Running));
}

TEST_CASE("platform_module_headless_window_is_null", "[platform]") {
    HeadlessPlatformFixture f;
    REQUIRE(f.mod.window() == nullptr);
}

TEST_CASE("platform_module_name_is_platform", "[platform]") {
    HeadlessPlatformFixture f;
    REQUIRE(std::string(f.mod.name()) == std::string("Platform"));
}

TEST_CASE("platform_current_accessible_after_init", "[platform]") {
    HeadlessPlatformFixture f;
    // IPlatform::current() no debe disparar el assert — simplemente se llama.
    auto& plat = IPlatform::current();
    REQUIRE(std::string(plat.name()).size() > 0);
}

// Init con ventana (crea una ventana Win32 real y la cierra de inmediato) --------------------------

TEST_CASE("platform_windowed_window_is_not_null", "[platform]") {
    PlatformModule::Config cfg;
    cfg.window = { "PAL Test Window", 640, 480, false };

    anxiety::EngineConfig eCfg;
    eCfg.app_name = "PAL Windowed Test";
    eCfg.headless = false;

    anxiety::Engine engine(eCfg);
    auto& platMod = engine.emplace_module<PlatformModule>(cfg);

    REQUIRE(engine.init());

    IWindow* win = platMod.window();
    REQUIRE(win != nullptr);
    REQUIRE(win->is_open());
    REQUIRE(win->width() > 0);
    REQUIRE(win->height() > 0);

    // Se cierra de inmediato para que la ventana no bloquee la ejecución del test.
    win->close();
    (void)win->poll_events();                       // vacía WM_CLOSE → marca m_open = false

    engine.shutdown();
}

TEST_CASE("platform_windowed_native_handle_is_hwnd", "[platform]") {
    PlatformModule::Config cfg;
    cfg.window = { "HWND Test", 320, 240, false };

    anxiety::EngineConfig eCfg;
    eCfg.headless = false;

    anxiety::Engine engine(eCfg);
    auto& platMod = engine.emplace_module<PlatformModule>(cfg);

    REQUIRE(engine.init());

    void* handle = platMod.window()->native_handle();
    REQUIRE(handle != nullptr);

    platMod.window()->close();
    (void)platMod.window()->poll_events();
    engine.shutdown();
}
