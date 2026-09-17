#pragma once

#include "RHITypes.h"

#include <cstdint>

namespace anxiety::rendering::rhi {
    class IPipeline;                                // fwd - evita includes pesados en esta cabecera
    class IDescriptorSet;                           // fwd

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
        virtual void clear_depth_stencil(TextureHandle depth, float depth_val = 1.f, uint8_t stencil = 0) = 0;

        // Vinculación de estado de pipeline --------------------------------------------------------
        // Vincula un objeto de estado de pipeline (PSO) inmutable. Debe llamarse antes de cualquier
        // draw call. También fija la topología de primitivas y el root signature.
        virtual void bind_pipeline(IPipeline& pipeline) = 0;
        // Vincula un descriptor set en el índice de set dado. El set debe haberse creado con el
        // mismo layout que el pipeline vinculado.
        virtual void bind_descriptor_set(uint32_t set, IDescriptorSet& descriptor_set) = 0;

        // Vertex / index buffers -----------------------------------------------------------------
        // Vincula un vertex buffer al slot de input-assembler indicado.
        //   stride - tamaño en bytes de un registro de vértice. Debe coincidir con VertexLayout::stride_bytes del pipeline para este slot.
        virtual void bind_vertex_buffer(uint32_t slot, BufferHandle handle, uint64_t offset = 0, uint32_t stride = 0) = 0;
        // Vincula un index buffer. use_32_bit selecciona R32_UINT (true) o R16_UINT (false).
        virtual void bind_index_buffer(BufferHandle handle, uint64_t offset = 0, bool use_32_bit = true) = 0;

        // Draw / dispatch ------------------------------------------------------------------------
        // Provisionales — todavía no existe un sistema de pipeline-state object.
        virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0)                                  = 0;
        virtual void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) = 0;
        
        virtual void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) = 0;

    protected:
        ICommandBuffer() = default;
        ICommandBuffer(const ICommandBuffer&) = delete;
        ICommandBuffer& operator=(const ICommandBuffer&) = delete;
    };
} // namespace anxiety::rendering::rhi
