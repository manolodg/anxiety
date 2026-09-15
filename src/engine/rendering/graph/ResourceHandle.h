#pragma once

#include <cstdint>

namespace anxiety::rendering::graph {
    // Handles de recursos del render graph ---------------------------------------------------------
    // Estos handles a nivel de grafo son distintos de rhi::TextureHandle / rhi::BufferHandle. Identifican
    // recursos declarados dentro de una instancia de RenderGraph (que pueden ser virtuales / transitorios
    // antes de compile()). Tras compile(), cada recurso vivo del grafo queda respaldado por un handle físico de la RHI.
    // --------------------------------------------------------------------------------------------
    struct RGTextureHandle {
        uint32_t id = 0;

        [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
    };

    struct RGBufferHandle {
        uint32_t id = 0;
        [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
    };
} // anxiety::rendering::graph