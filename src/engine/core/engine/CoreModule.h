#pragma once

#include "IModule.h"

namespace anxiety {
    // CoreModule -----------------------------------------------------------------------------------
    // IModule mínimo cuya única responsabilidad es registrar, en el EngineRegistry del motor
    // (engine.registry()), el módulo "core" y sus componentes fundamentales — hoy solo
    // ecs::components::Transform, como "core.transform". No posee ningún estado de ECS por sí
    // mismo (no crea un World, no gestiona entidades): eso sigue siendo responsabilidad de quien use
    // ecs::World directamente. Ver docs/anxiety/architecture/ecs-registry.md.
    //
    // Registrado por cualquier consumidor del motor (bridge, sandbox...) igual que PlatformModule o
    // RenderingModule — no se auto-registra: sigue el mismo principio de "registro explícito" que el
    // resto de módulos del motor.
    // --------------------------------------------------------------------------------------------
    class CoreModule final : public IModule {
    public:
        [[nodiscard]] std::string_view name() const noexcept override { return "Core"; }

        [[nodiscard]] bool on_init(Engine& engine) override;
        void               on_update(float delta)  override;
        void               on_shutdown()           override;
    };
} // namespace anxiety
