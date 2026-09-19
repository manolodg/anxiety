#pragma once

#include "World.h"
#include "IModule.h"

namespace anxiety::ecs {
	// EcsModule ----------------------------------------------------------------------------------
	// Envoltorio IModule alrededor de World. Regístralo en el motor para tener un World de ECS
	// gestionado cuya vida está ligada al ciclo init/shutdown del motor.
	//
	//   auto& ecs = engine.emplaceModule<EcsModule>();
	//   // Tras engine.init():
	//   World& world = ecs.world();
	// --------------------------------------------------------------------------------------------
	class EcsModule final : public anxiety::IModule {
	public:
		[[nodiscard]] std::string_view name()                           const noexcept override { return "ECS"; }
		[[nodiscard]] bool             on_init(anxiety::Engine& engine)                override;

		void on_update(float dt) override;
		void on_shutdown()       override;

		[[nodiscard]] World&       world()       noexcept { return m_world; }
		[[nodiscard]] const World& world() const noexcept { return m_world; }

	private:
		World m_world;
	};
} // namespace anxiety::ecs