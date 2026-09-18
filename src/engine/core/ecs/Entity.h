#pragma once

#include <cstdint>
#include <limits>

namespace anxiety::ecs {
	// EntityId -----------------------------------------------------------------------------------
	//	Handle de 64 bits: los 32 bits superiores son la generación y los 32 bits inferiores el índice.
	//	Una entidad nula se representa mediante EntityId::null().
	// --------------------------------------------------------------------------------------------
	struct EntityId {
		static constexpr uint32_t k_invalid_index = std::numeric_limits<uint32_t>::max();

		uint32_t index      = k_invalid_index;
        uint32_t generation = 0;

        [[nodiscard]] constexpr bool is_valid() const noexcept { return index != k_invalid_index; }

        [[nodiscard]] static constexpr EntityId null() noexcept { return EntityId{}; }

        constexpr bool operator==(const EntityId&) const noexcept = default;
        constexpr bool operator!=(const EntityId&) const noexcept = default;
	};
} // namespace anxiety::ecs