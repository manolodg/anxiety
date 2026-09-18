#pragma once

#ifdef ANXIETY_BACKEND_GLES

// El backend de GLES se construye sobre el backend de OpenGL — hereda de GLDevice y sobrescribe
// el subconjunto de métodos que difieren entre GL 4.x y GLES 3.x.
//
// Diferencias clave respecto a GL de escritorio:
//   - compile_shader_from_source() antepone "#version 300 es" + calificadores de precisión
//   - EGL gestiona el contexto de GL (a través de GLESSwapchain)
//   - Varios puntos de entrada de GL 4.x (polygon mode, draw con base-instance, etc.) están
//     ausentes; se protegen en tiempo de ejecución en GLCommandBuffer.
#include "../opengl/GLDevice.h"

namespace anxiety::rendering::backend::gles {
    // GLESDevice — implementación de OpenGL ES 3.x --------------------------------------------------
    //
    // Hereda toda la maquinaria de gestión de recursos y grabación de comandos de GLDevice. Solo
    // sobrescribe las partes que difieren en GLES.
    // --------------------------------------------------------------------------------------------
    class GLESDevice final : public opengl::GLDevice {
    public:
        explicit GLESDevice(bool enable_validation = false);
        ~GLESDevice() override;

        // Debe llamarse una vez que haya un contexto EGL actual (después de crear el primer GLESSwapchain).
        void init_after_context();

        // Sobrescrituras de IDevice ---------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "OpenGL ES"; }

        // Antepone "#version 300 es" + calificadores de precisión al source del shader antes de
        // devolver los bytes de source para compilación — GLES 3.0 exige la directiva de versión.
        [[nodiscard]] std::vector<uint8_t> compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) override;

        // Crea un GLESSwapchain respaldado por EGL en lugar de una superficie de plataforma WGL/GLX.
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain> create_swapchain(const rhi::SwapchainDesc& desc) override;

    private:
        bool m_enable_validation = false;
    };
} // namespace anxiety::rendering::backend::gles

#endif // ANXIETY_BACKEND_GLES
