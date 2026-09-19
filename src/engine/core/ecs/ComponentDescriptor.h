#pragma once

#include "PropertyDescriptor.h"

#include <string>
#include <vector>

namespace anxiety::ecs {
    // ComponentDescriptor ----------------------------------------------------------------------------
    // Describe un tipo de componente ECS registrado para que el editor pueda descubrirlo — no es el
    // componente en sí (eso sigue siendo un struct de C++ normal, almacenado por World mediante
    // ComponentRegistry::type_id<T>()/ComponentStorage<T>): es su metadato, independiente de esa
    // representación en memoria. Ambos sistemas coexisten a propósito, ver EngineRegistry.h.
    // --------------------------------------------------------------------------------------------
    struct ComponentDescriptor {
        // Identificador estable y con espacio de nombres por módulo (p. ej. "core.transform").
        // Independiente del idioma — es lo que viaja por el bridge y lo que persistiría en una
        // futura escena serializada, nunca el Name.
        std::string                      id;
        // Nombre para mostrar en la UI (p. ej. "Transform").
        std::string                      name;
        std::vector<PropertyDescriptor>  properties;
    };
} // namespace anxiety::ecs
