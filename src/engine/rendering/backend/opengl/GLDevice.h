#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

#include "GLHelpers.h"
#include "../../rhi/IDevice.h"

#include <memory>
#include <string_view>
#include <vector>

namespace anxiety::rendering::backend::opengl {

    // GLTextureSlot — una entrada en el pool de recursos de texturas del device -------------------
    struct GLTextureSlot {
        GLuint   name            = 0;
        uint32_t width           = 0;
        uint32_t height          = 0;
        uint32_t mips            = 1;
        GLenum   internal_format = GL_RGBA8;
        GLenum   base_format     = GL_RGBA;
        GLenum   pixel_type      = GL_UNSIGNED_BYTE;
        bool     is_depth        = false;
        bool     alive           = false;
    };

    // GLBufferSlot — una entrada en el pool de recursos de buffers del device ----------------------
    struct GLBufferSlot {
        GLuint   name   = 0;
        GLenum   target = GL_ARRAY_BUFFER;
        uint64_t size   = 0;
        bool     alive  = false;
    };

    // GLDevice — implementación de OpenGL 4.x de rhi::IDevice --------------------------------------
    // Gestión del contexto: se pasa native_window (HWND en Win32, Window XID en X11) al constructor
    // para que GLDevice pueda crear y poseer el contexto de renderizado de la plataforma. GLSwapchain
    // solo maneja el intercambio de buffers (NO posee el contexto).
    // --------------------------------------------------------------------------------------------
    class GLDevice : public rhi::IDevice {
    public:
        // native_window: HWND en Win32, X11 Window en Linux, nullptr si el contexto ya está activo.
        explicit GLDevice(bool enable_validation = false, void* native_window = nullptr);
        ~GLDevice() override;

        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice -----------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "OpenGL"; }

        [[nodiscard]] rhi::BufferHandle  create_buffer(const rhi::BufferDesc& desc, const void* initial_data = nullptr, size_t initial_data_sz = 0) override;
        [[nodiscard]] rhi::TextureHandle create_texture(const rhi::TextureDesc&) override;
        void destroy_buffer(rhi::BufferHandle)   override;
        void destroy_texture(rhi::TextureHandle) override;

        void write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size)                  override;
        void upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) override;

        [[nodiscard]] std::vector<uint8_t>                  compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) override;
        [[nodiscard]] std::unique_ptr<rhi::IShader>         create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) override;
        [[nodiscard]] std::unique_ptr<rhi::IPipeline>       create_pipeline(const rhi::PipelineDesc& desc) override;
        [[nodiscard]] std::unique_ptr<rhi::IDescriptorSet>  create_descriptor_set(const rhi::DescriptorSetLayout& layout) override;

        [[nodiscard]] std::unique_ptr<rhi::ICommandBuffer> create_command_buffer()                                         override;
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain>     create_swapchain(const rhi::SwapchainDesc&) override;

        void submit(rhi::ICommandBuffer&) override;
        void wait_idle()                                      override;

        // Accesores internos (usados por el command buffer / descriptor set) -----------------------
        [[nodiscard]] GLTextureSlot&       tex_slot(rhi::TextureHandle h);
        [[nodiscard]] const GLTextureSlot& tex_slot(rhi::TextureHandle h) const;
        [[nodiscard]] GLBufferSlot&        buf_slot(rhi::BufferHandle  h);
        [[nodiscard]] const GLBufferSlot&  buf_slot(rhi::BufferHandle  h) const;

    protected:
        struct DeferGLAD {};
        // Constructor para subclases que gestionan su propia carga de contexto+GLAD (p. ej. GLESDevice).
        // Omite gladLoadGL() — la subclase debe llamar a init_glad() tras activar un contexto.
        explicit GLDevice(DeferGLAD) noexcept {}

        void init_glad(bool enable_validation);

        bool m_valid = false;

    private:
        std::vector<GLTextureSlot> m_textures;
        std::vector<uint32_t>      m_texture_free;
        std::vector<GLBufferSlot>  m_buffers;
        std::vector<uint32_t>      m_buffer_free;

        [[nodiscard]] uint32_t alloc_tex_slot();
        void                   free_tex_slot(uint32_t idx);
        [[nodiscard]] uint32_t alloc_buf_slot();
        void                   free_buf_slot(uint32_t idx);

    #if defined(_WIN32)
        void* m_hglrc = nullptr;  // HGLRC
        void* m_hdc   = nullptr;  // HDC
        void* m_hwnd  = nullptr;  // HWND
    #elif defined(__APPLE__)
        void* m_nsgl_ctx = nullptr;  // NSOpenGLContext*
    #elif defined(__linux__) && !defined(__ANDROID__)
        void* m_display     = nullptr;  // Display*
        void* m_glx_ctx     = nullptr;  // GLXContext
        bool  m_own_display = false;
    #endif

        // Callback de mensajes de depuración de GL — se registra cuando enable_validation == true
        static void GLAPIENTRY gl_debug_callback(
            GLenum        source,
            GLenum        type,
            GLuint        id,
            GLenum        severity,
            GLsizei       length,
            const GLchar* message,
            const void*   user_param);
    };

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
