#pragma once

#include "RHITypes.h"

namespace anxiety::rendering::rhi {
    // ISwapchain --------------------------------------------------------------------------------
    // Gestiona la cadena de presentación (2–3 backbuffers). Se obtiene mediante
    // IDevice::create_swapchain().
    //
    // Uso por fotograma:
    //   swapchain.acquire_next_image();            // avanza al buffer actual
    //   auto bb = swapchain.current_backbuffer();  // TextureHandle de este fotograma
    //   /* graba el clear / draw sobre bb */
    //   swapchain.present();                       // lo encola para mostrarse en pantalla
    // --------------------------------------------------------------------------------------------
    class ISwapchain {
    public:
        virtual ~ISwapchain() = default;

        // Consultas -------------------------------------------------------------------------------
        [[nodiscard]] virtual uint32_t     image_count()         const noexcept = 0;
        [[nodiscard]] virtual Extent2D     extent()              const noexcept = 0;
        [[nodiscard]] virtual Format       format()              const noexcept = 0;

        // Devuelve el TextureHandle del backbuffer del fotograma actual. Válido entre acquire_next_image() y present().
        [[nodiscard]] virtual TextureHandle current_backbuffer() const noexcept = 0;

        // Operaciones por fotograma ----------------------------------------------------------------
        // Avanza el índice interno hasta el siguiente backbuffer disponible. Devuelve el índice de la imagen (0 .. imageCount-1).
        virtual uint32_t acquire_next_image() = 0;

        // Encola el backbuffer actual para mostrarse en pantalla.
        virtual void     present() = 0;

        // Redimensiona el swapchain cuando cambia el tamaño de la ventana.
        virtual void     resize(Extent2D newExtent) = 0;

    protected:
        ISwapchain() = default;
        ISwapchain(const ISwapchain&) = delete;
        ISwapchain& operator=(const ISwapchain&) = delete;
    };
} // namespace anxiety::rendering::rhi