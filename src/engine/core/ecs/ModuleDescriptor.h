#pragma once

#include <string>
#include <vector>

namespace anxiety::ecs {
    // ModuleDescriptor -------------------------------------------------------------------------------
    // Describe, para el editor, un módulo del motor que registra componentes — no debe confundirse
    // con IModule (el módulo real del ciclo de vida del motor): un IModule *registra* su
    // ModuleDescriptor durante on_init(), pero este struct es solo el metadato resultante, sin
    // comportamiento ni referencia al IModule que lo creó. Varios IModule podrían en teoría compartir
    // un mismo ModuleDescriptor si algún día tuviera sentido, aunque hoy es 1:1.
    // --------------------------------------------------------------------------------------------
    struct ModuleDescriptor {
        // Identificador estable, independiente del idioma (p. ej. "core", "graphics2d", "physics").
        // Es el prefijo de espacio de nombres de los ComponentDescriptor::id que le pertenecen.
        std::string              id;
        // Nombre para mostrar en la UI (p. ej. "Core").
        std::string              name;
        // Ids de los ComponentDescriptor que pertenecen a este módulo — no los descriptors
        // completos, para no duplicar su contenido; EngineRegistry::component(id) resuelve cada uno.
        std::vector<std::string> components;
    };
} // namespace anxiety::ecs
