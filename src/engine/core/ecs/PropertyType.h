#pragma once

#include <cstdint>
#include <string_view>

namespace anxiety::ecs {
    // PropertyType ---------------------------------------------------------------------------------
    // Qué clase de dato describe un PropertyDescriptor::type. No es el tipo de C++ real del campo
    // (eso es un detalle interno del componente) — es la información mínima que un futuro Inspector
    // del editor necesita para decidir qué control mostrar (una casilla, un campo numérico, un
    // selector de color...). Ampliable: añade un valor nuevo aquí y su caso en to_string() cuando el
    // motor lo necesite; no hace falta anticipar tipos que ningún componente usa todavía.
    // --------------------------------------------------------------------------------------------
    enum class PropertyType : uint8_t {
        Bool,
        Int,
        Float,
        Double,
        String,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Enum,
        AssetReference,
    };

    // Nombre estable (independiente del idioma) usado al serializar hacia el editor — ver
    // EngineRegistry::to_json().
    [[nodiscard]] constexpr std::string_view to_string(PropertyType type) noexcept {
        switch (type) {
        case PropertyType::Bool:           return "bool";
        case PropertyType::Int:            return "int";
        case PropertyType::Float:          return "float";
        case PropertyType::Double:         return "double";
        case PropertyType::String:         return "string";
        case PropertyType::Vector2:        return "vector2";
        case PropertyType::Vector3:        return "vector3";
        case PropertyType::Vector4:        return "vector4";
        case PropertyType::Color:          return "color";
        case PropertyType::Enum:           return "enum";
        case PropertyType::AssetReference: return "assetReference";
        }
        return "unknown";
    }
} // namespace anxiety::ecs
