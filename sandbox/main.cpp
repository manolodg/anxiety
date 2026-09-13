#include "Logger.h"

#include <string_view>

using namespace anxiety;

static constexpr std::string_view k_category = "sandbox";

int main() {
	logs::Logger::get().set_level(logs::LogLevel::Info);
	LOG_INFO(k_category, "prueba");

	return 0;
}