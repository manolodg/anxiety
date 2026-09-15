#pragma once

#include "RHITypes.h"

namespace anxiety::rendering::rhi {
    // ITexture -----------------------------------------------------------------------------------
    // Interfaz extendida para texturas cuando se necesita acceso directo al objeto. La mayoría de
    // los usuarios interactúan con las texturas a través de TextureHandle + IDevice.
    // --------------------------------------------------------------------------------------------
    class ITexture {
    public:
        virtual ~ITexture() = default;

        [[nodiscard]] virtual TextureHandle handle() const noexcept = 0;
        [[nodiscard]] virtual Extent2D      extent() const noexcept = 0;
        [[nodiscard]] virtual Format        format() const noexcept = 0;

    protected:
        ITexture() = default;
        ITexture(const ITexture&) = delete;
        ITexture& operator=(const ITexture&) = delete;
    };
} // namespace anxiety::rendering::rhi