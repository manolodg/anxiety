#pragma once

#include <cstdint>
#include <typeindex>
#include <unordered_map>
#include <atomic>

namespace anxiety::ecs {
    using ComponentTypeId = uint32_t;

    static constexpr ComponentTypeId k_invalid_component_type_id = 0;

    // ComponentRegistry --------------------------------------------------------------------------
    //	Asigna un ID numérico estable a cada tipo de componente en tiempo de ejecución.
    //	Los IDs empiezan en 1 para que el 0 pueda usarse como "inválido".
    // --------------------------------------------------------------------------------------------
    class ComponentRegistry {
    public:
        template<typename T>
        static ComponentTypeId type_id() noexcept {
            static const ComponentTypeId id = next_id();
            return id;
        }

    private:
        static ComponentTypeId next_id() noexcept {
            static std::atomic<ComponentTypeId> counter{ 1 };
            return counter.fetch_add(1, std::memory_order_relaxed);
        }
    };
} // namespace anxiety::ecs