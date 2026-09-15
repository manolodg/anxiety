#pragma once

#include "RHITypes.h"

#include <memory>
#include <string_view>

namespace anxiety::rendering::rhi {
    class ICommandBuffer;
    class ISwapchain;

    // IDevice ------------------------------------------------------------------------------------
    // Handle central del dispositivo de GPU. Es propietario del ciclo de vida de todos los recursos
    // de GPU. Un IDevice por cada adaptador de GPU físico (o virtual).
    //
    // Uso típico por fotograma:
    //   swapchain->acquire_next_image();
    //   cmd->begin();
    //     cmd->resource_barrier(bb, Present, RenderTarget);
    //     cmd->clear_render_target(bb, clearColor);
    //     cmd->resource_barrier(bb, RenderTarget, Present);
    //   cmd->end();
    //   device->submit(*cmd);
    //   swapchain->present();
    //   device->waitIdle();
    //
    // Esta interfaz no expone ningún tipo específico de una API concreta.
    // --------------------------------------------------------------------------------------------
    class IDevice {
    public:
        virtual ~IDevice() = default;

        [[nodiscard]] virtual std::string_view backend_name() const noexcept = 0;

        // Creación / destrucción de recursos -----------------------------------------------------
        [[nodiscard]] virtual BufferHandle  create_buffer(const BufferDesc&) = 0;
        [[nodiscard]] virtual TextureHandle create_texture(const TextureDesc&) = 0;
        virtual void destroy_buffer(BufferHandle) = 0;
        virtual void destroy_texture(TextureHandle) = 0;

        // Grabación de comandos -------------------------------------------------------------------
        [[nodiscard]] virtual std::unique_ptr<ICommandBuffer> create_command_buffer() = 0;

        // Swapchain ------------------------------------------------------------------------------
        // Devuelve nullptr si el backend no soporta presentación (modo headless).
        [[nodiscard]] virtual std::unique_ptr<ISwapchain> create_swapchain(const SwapchainDesc&) = 0;

        // Envío y sincronización -------------------------------------------------------------------
        virtual void submit(ICommandBuffer&) = 0;

        // Bloquea el hilo que llama hasta que todo el trabajo de GPU en curso haya terminado.
        virtual void wait_idle() = 0;

    protected:
        IDevice() = default;
        IDevice(const IDevice&) = delete;
        IDevice& operator=(const IDevice&) = delete;
    };
} // namespace anxiety::rendering::rhi