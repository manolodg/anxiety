#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

#include "GLHelpers.h"
#include "../../rhi/ICommandBuffer.h"

#include <functional>
#include <vector>

namespace anxiety::rendering::backend::opengl {

    class GLDevice;   // forward
    class GLPipeline; // forward

    // GLCommandBuffer — grabación diferida de comandos para OpenGL de escritorio -----------------
    // OpenGL es inherentemente de modo inmediato, pero el contrato de la RHI exige que los comandos
    // se graben en un buffer y se envíen más tarde mediante IDevice::submit().
    //
    // Cada método de ICommandBuffer agrega una lambda std::function<void()> a m_commands.
    // GLDevice::submit() llama a execute_all(), que reproduce esas lambdas en orden sobre el
    // contexto de GL actual.
    //
    // El formato de los atributos de vértice se aplica en tiempo de ejecución en la lambda de
    // bind_vertex_buffer usando glVertexAttribPointer contra el VAO actualmente vinculado
    // (establecido por la lambda de bind_pipeline anterior). Esto evita el DSA de GL 4.5 y
    // funciona en macOS 4.1 y GLES 3.0.
    // --------------------------------------------------------------------------------------------
    class GLCommandBuffer final : public rhi::ICommandBuffer {
    public:
        explicit GLCommandBuffer(GLDevice* device) noexcept : m_device(device) {}
        ~GLCommandBuffer() override = default;

        // ICommandBuffer ---------------------------------------------------------------------------
        void begin() override;
        void end()   override;

        void resource_barrier(rhi::TextureHandle texture, rhi::ResourceState before, rhi::ResourceState after) override;

        void clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color) override;

        void clear_depth_stencil(rhi::TextureHandle depth, float depth_val = 1.f, uint8_t stencil = 0) override;

        void bind_pipeline(rhi::IPipeline& pipeline) override;

        void bind_descriptor_set(uint32_t set, rhi::IDescriptorSet& descriptor_set) override;

        void bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset = 0, uint32_t stride = 0) override;

        void bind_index_buffer(rhi::BufferHandle handle, uint64_t offset = 0, bool use_32_bit = true) override;

        void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) override;

        void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) override;

        void dispatch(uint32_t groups_X, uint32_t groups_Y, uint32_t groups_Z) override;

        // Específico de GL -------------------------------------------------------------------------
        // Reproduce todos los comandos grabados sobre el contexto de GL actual. Llamado por GLDevice::submit().
        void execute_all();

    private:
        GLDevice*                          m_device          = nullptr;
        std::vector<std::function<void()>> m_commands;
        bool                               m_recording       = false;

        // Estado cacheado por grabación — se establece en tiempo de grabación y se captura por valor en las lambdas.
        GLPipeline* m_active_pipeline       = nullptr;
        GLuint      m_active_vao            = 0;
        GLenum      m_index_type            = GL_UNSIGNED_INT;
        uint64_t    m_index_buffer_offset   = 0;
        GLenum      m_topology              = GL_TRIANGLES;

        // Helper: crea y vincula un FBO temporal para limpiar una textura de render target que no sea la por defecto.
        void push_clear_FBO(rhi::TextureHandle rt, const rhi::ClearColor& color);
    };

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
