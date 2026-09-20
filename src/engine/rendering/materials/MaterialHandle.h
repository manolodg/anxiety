#pragma once

#include <cstdint>

// MaterialHandle / MaterialInstanceHandle --------------------------------------------------------
// Handles opacos, ligeros y trivialmente copiables. Aptos para guardarse dentro de componentes de
// ECS sin arrastrar las cabeceras completas del sistema de materiales. id == 0  →  inválido
// (construido por defecto).
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::materials {
    struct MaterialHandle {
        uint32_t id = 0;

        [[nodiscard]] constexpr bool is_valid() const noexcept { return id != 0; }
        constexpr bool operator==(const MaterialHandle&)  const noexcept = default;
    };

    struct MaterialInstanceHandle {
        uint32_t id = 0;

        [[nodiscard]] constexpr bool is_valid() const noexcept { return id != 0; }
        constexpr bool operator==(const MaterialInstanceHandle&) const noexcept = default;
    };
} // namespace anxiety::rendering::materials
