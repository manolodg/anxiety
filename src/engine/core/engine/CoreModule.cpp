#include "CoreModule.h"
#include "Engine.h"
#include "Logger.h"

// Registra los metadatos de ecs::components::Transform (ver components/Transform.h) — no se incluye
// aquí ese header porque este módulo solo trata con el metadato (ComponentDescriptor), nunca con el
// tipo de C++ en sí; ningún World se crea ni se toca desde aquí.
namespace anxiety {
    static constexpr std::string_view k_category = "Core";

    bool CoreModule::on_init(Engine& engine) {
        ecs::EngineRegistry& registry = engine.registry();

        registry.register_module({ .id = "core", .name = "Core" });

        registry.register_component("core", {
            .id = "core.transform",
            .name = "Transform",
            .properties = {
                { .id = "position", .name = "Position", .type = ecs::PropertyType::Vector3 },
                { .id = "rotation", .name = "Rotation", .type = ecs::PropertyType::Vector3 },
                { .id = "scale",    .name = "Scale",    .type = ecs::PropertyType::Vector3 },
            }
        });

        LOG_INFO(k_category, "Módulo 'core' registrado en el EngineRegistry (core.transform).");
        return true;
    }

    void CoreModule::on_update(float /*delta*/) {}

    void CoreModule::on_shutdown() {
        LOG_INFO(k_category, "CoreModule desconectado.");
    }
} // namespace anxiety
