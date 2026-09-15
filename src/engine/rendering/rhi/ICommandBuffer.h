#pragma once

#include "RHITypes.h"

namespace anxiety::rendering::rhi {
    // ICommandBuffer -----------------------------------------------------------------------------
    // Una única grabación de comandos de GPU. Los comandos se encolan en memoria del host y solo
    // se despachan a la GPU cuando el buffer se envía mediante IDevice::submit().
    //
    // Ciclo de vida por fotograma:
    //   begin()  →  [ graba comandos ]  →  end()  →  IDevice::submit()
    //
    // Tras que submit() retorne, begin() puede volver a llamarse para reutilizar el buffer. Es
    // comportamiento indefinido llamar a cualquier método de grabación fuera de begin/end.
    // --------------------------------------------------------------------------------------------
    class ICommandBuffer {
    public:
        virtual ~ICommandBuffer() = default;

        // Ciclo de vida de la grabación -------------------------------------------------------------
        virtual void begin() = 0;
        virtual void end() = 0;

        // Barreras de recursos ---------------------------------------------------------------------
        // Inserta una barrera de pipeline / transición de estado del recurso.
        virtual void resource_barrier(TextureHandle texture, ResourceState before, ResourceState after) = 0;

        // Operaciones sobre el render target ---------------------------------------------------------
        virtual void clear_render_target(TextureHandle rt, const ClearColor& color) = 0;

        // Draw / dispatch ------------------------------------------------------------------------
        // Provisionales — todavía no existe un sistema de pipeline-state object.
        virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;
        virtual void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) = 0;

    protected:
        ICommandBuffer() = default;
        ICommandBuffer(const ICommandBuffer&) = delete;
        ICommandBuffer& operator=(const ICommandBuffer&) = delete;
    };
} // namespace anxiety::rendering::rhi
