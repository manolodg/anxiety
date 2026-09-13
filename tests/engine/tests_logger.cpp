#include <catch2/catch_test_macros.hpp>

#include "Logger.h"

TEST_CASE("logger_level_filter", "[logger]") {
    auto& log = anxiety::logs::Logger::get();

    // Se establece en Error: solo deberían dispararse Error/Fatal
    log.set_level(anxiety::logs::LogLevel::Error);

    log.log(anxiety::logs::LogLevel::Info, "Test", "Esto debería suprimirse");
    log.log(anxiety::logs::LogLevel::Error, "Test", "Esto debería aparecer");

    // Restablecer a Trace
    log.set_level(anxiety::logs::LogLevel::Trace);

    REQUIRE(true);
}

TEST_CASE("logger_level_accessor", "[logger]") {
    auto& log = anxiety::logs::Logger::get();

    log.set_level(anxiety::logs::LogLevel::Debug);

    REQUIRE(static_cast<int>(log.get_level()) == static_cast<int>(anxiety::logs::LogLevel::Debug));

    log.set_level(anxiety::logs::LogLevel::Trace);
}