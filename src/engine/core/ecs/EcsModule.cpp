#include "EcsModule.h"
#include "Logger.h"

namespace anxiety::ecs {
	static constexpr std::string_view k_category = "ECS";

	bool EcsModule::on_init(Engine& /*engine*/) {
		LOG_INFO(k_category, "Módulo ECS en línea (basado en archetypes, layout SoA).");
		return true;
	}

	void EcsModule::on_update(float /*dt*/) {
		// Los sistemas se controlan explícitamente por quien usa World::query(). Una futura
		// iteración podría ejecutar aquí funciones de sistema registradas.
	}

	void EcsModule::on_shutdown() { LOG_INFO(k_category, "Módulo ECS desconectado."); }
} // namespace anxiety::ecs