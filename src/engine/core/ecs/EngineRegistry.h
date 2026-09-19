#pragma once

#include "ComponentDescriptor.h"
#include "ModuleDescriptor.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace anxiety::ecs {
    // EngineRegistry -----------------------------------------------------------------------------
    // Autoridad del motor sobre qué módulos, componentes y propiedades existen en esta build — el
    // contrato de descubrimiento entre el Engine y el Editor. Filosofía: "automatic discovery,
    // explicit definition" — cada IModule registra explícitamente sus propios ComponentDescriptor
    // durante on_init() (ver CoreModule::on_init() para el primer ejemplo, con Core.Transform); el
    // Editor los descubre consultando este registry, sin mantener ninguna lista propia hardcodeada.
    //
    // Deliberadamente independiente de ComponentRegistry (ecs/ComponentRegistry.h): aquella clase
    // asigna un ComponentTypeId numérico y NO estable entre ejecuciones a cada tipo de C++ para que
    // World pueda indexar su almacenamiento — es un detalle interno de rendimiento. Este registry
    // resuelve un problema distinto: identidad ESTABLE (ids de cadena, iguales en cada build) y
    // metadatos (nombre, propiedades) para que un llamador externo (el editor, vía el bridge) pueda
    // describir la composición de la build sin conocer los tipos de C++ en absoluto.
    //
    // Un Engine posee exactamente un EngineRegistry (ver Engine::registry()), construido junto con
    // el motor y disponible antes de init() para que los IModule puedan registrar en su on_init().
    //
    // Thread safety: pensado para poblarse durante on_init() (un solo hilo, antes de run()) y
    // consultarse después desde cualquier hilo sin mutación concurrente — igual que m_sorted_order
    // en Engine. No protege escrituras concurrentes con lecturas.
    // --------------------------------------------------------------------------------------------
    class EngineRegistry {
    public:
        // Registra un módulo nuevo. Rechaza (con un error de log, sin lanzar) un id ya registrado.
        void register_module(ModuleDescriptor module);

        // Registra un componente y lo asocia al módulo module_id, que debe haberse registrado antes
        // con register_module(). Rechaza (con un error de log, sin lanzar) un id de componente ya
        // registrado o un module_id desconocido.
        void register_component(std::string_view module_id, ComponentDescriptor component);

        // Consultas — GetModules() / GetComponents() / GetComponent(id) / GetProperties(componentId).
        [[nodiscard]] const std::vector<ModuleDescriptor>&    modules()    const noexcept { return m_modules; }
        // Todos los componentes registrados, de cualquier módulo, en orden de registro.
        [[nodiscard]] const std::vector<ComponentDescriptor>& components() const noexcept { return m_components; }
        // nullptr si no existe ningún componente con ese id.
        [[nodiscard]] const ComponentDescriptor*              component(std::string_view id) const noexcept;
        // nullptr si no existe ningún componente con ese id (distinto de "existe pero sin propiedades").
        [[nodiscard]] const std::vector<PropertyDescriptor>*  properties(std::string_view component_id) const noexcept;

        // Serializa el estado completo del registry a JSON, para cruzar el bridge hacia C# (ver
        // anxiety_bridge_get_registry_json() en src/bridge/). Forma:
        //   { "modules": [ { "id", "name", "components": [id, ...] }, ... ],
        //     "components": [ { "id", "name", "properties": [ { "id", "name", "type" }, ... ] }, ... ] }
        [[nodiscard]] std::string to_json() const;

    private:
        std::vector<ModuleDescriptor>              m_modules;
        std::vector<ComponentDescriptor>           m_components;
        std::unordered_map<std::string, size_t>    m_module_index;      // id -> índice en m_modules
        std::unordered_map<std::string, size_t>    m_component_index;   // id -> índice en m_components
    };
} // namespace anxiety::ecs
