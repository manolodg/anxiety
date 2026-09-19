#include "EngineRegistry.h"
#include "Logger.h"

namespace anxiety::ecs {
    static constexpr std::string_view k_category = "EngineRegistry";

    namespace {
        // Escapa una cadena para incrustarla entre comillas dobles en el JSON servido por to_json().
        // Los ids/nombres de este registry son literales de código controlados por el propio motor
        // (nunca entrada de usuario), así que basta con cubrir los caracteres que romperían el JSON.
        void append_json_escaped(std::string& out, std::string_view value) {
            out += '"';
            for (const char c : value) {
                switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
                }
            }
            out += '"';
        }
    } // namespace

    void EngineRegistry::register_module(ModuleDescriptor module) {
        if (m_module_index.contains(module.id)) {
            LOGF_ERROR(k_category, "Ya hay un módulo registrado con el id '{}' — rechazado.", module.id);
            return;
        }

        m_module_index.emplace(module.id, m_modules.size());
        m_modules.push_back(std::move(module));
    }

    void EngineRegistry::register_component(std::string_view module_id, ComponentDescriptor component) {
        const auto module_it = m_module_index.find(std::string(module_id));
        if (module_it == m_module_index.end()) {
            LOGF_ERROR(k_category, "register_component('{}'): el módulo '{}' no está registrado.", component.id, module_id);
            return;
        }

        if (m_component_index.contains(component.id)) {
            LOGF_ERROR(k_category, "Ya hay un componente registrado con el id '{}' — rechazado.", component.id);
            return;
        }

        m_modules[module_it->second].components.push_back(component.id);
        m_component_index.emplace(component.id, m_components.size());
        m_components.push_back(std::move(component));
    }

    const ComponentDescriptor* EngineRegistry::component(std::string_view id) const noexcept {
        const auto it = m_component_index.find(std::string(id));
        return (it != m_component_index.end()) ? &m_components[it->second] : nullptr;
    }

    const std::vector<PropertyDescriptor>* EngineRegistry::properties(std::string_view component_id) const noexcept {
        const ComponentDescriptor* c = component(component_id);
        return c ? &c->properties : nullptr;
    }

    std::string EngineRegistry::to_json() const {
        std::string json;
        json.reserve(256 + m_components.size() * 96);

        json += "{\"modules\":[";
        for (size_t i = 0; i < m_modules.size(); ++i) {
            if (i > 0) json += ',';
            const ModuleDescriptor& m = m_modules[i];

            json += "{\"id\":";
            append_json_escaped(json, m.id);
            json += ",\"name\":";
            append_json_escaped(json, m.name);
            json += ",\"components\":[";
            for (size_t j = 0; j < m.components.size(); ++j) {
                if (j > 0) json += ',';
                append_json_escaped(json, m.components[j]);
            }
            json += "]}";
        }
        json += "],\"components\":[";

        for (size_t i = 0; i < m_components.size(); ++i) {
            if (i > 0) json += ',';
            const ComponentDescriptor& c = m_components[i];

            json += "{\"id\":";
            append_json_escaped(json, c.id);
            json += ",\"name\":";
            append_json_escaped(json, c.name);
            json += ",\"properties\":[";
            for (size_t j = 0; j < c.properties.size(); ++j) {
                if (j > 0) json += ',';
                const PropertyDescriptor& p = c.properties[j];

                json += "{\"id\":";
                append_json_escaped(json, p.id);
                json += ",\"name\":";
                append_json_escaped(json, p.name);
                json += ",\"type\":";
                append_json_escaped(json, to_string(p.type));
                json += '}';
            }
            json += "]}";
        }
        json += "]}";

        return json;
    }
} // namespace anxiety::ecs
