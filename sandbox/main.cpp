#include "Engine.h"
#include "Logger.h"

#include <string_view>

using namespace anxiety;

static constexpr std::string_view k_category = "sandbox";

int main() {
	logs::Logger::get().set_level(logs::LogLevel::Info);
	LOG_INFO(k_category, "prueba");

	Engine engine;
	if (!engine.init()) { LOG_FATAL(k_category, "No se ha podido iniciar el motor"); return -1; }
	engine.run();

	return 0;
}