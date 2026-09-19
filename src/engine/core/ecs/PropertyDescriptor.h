#pragma once

#include "PropertyType.h"

#include <string>

namespace anxiety::ecs {
    // PropertyDescriptor -----------------------------------------------------------------------------
    // Describe una propiedad editable de un ComponentDescriptor — metadato puro, sin ningún acceso
    // en vivo al valor real del campo (ningún getter/setter todavía): solo lo necesario para que un
    // futuro Inspector genérico sepa qué propiedad existe y qué tipo de control usar para editarla.
    // --------------------------------------------------------------------------------------------
    struct PropertyDescriptor {
        // Identificador estable dentro del componente (p. ej. "position"). Independiente del idioma
        // mostrado al usuario — nunca se traduce, nunca cambia entre builds.
        std::string  id;
        // Nombre para mostrar en la UI (p. ej. "Position"). Es el único campo que puede variar según
        // localización en el futuro; hoy es un literal en inglés/castellano según el componente.
        std::string  name;
        PropertyType type = PropertyType::Float;
    };
} // namespace anxiety::ecs
