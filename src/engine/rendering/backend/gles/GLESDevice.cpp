#ifdef ANXIETY_BACKEND_GLES

#include "GLESDevice.h"
#include "GLESSwapchain.h"
#include "Logger.h"

#include <EGL/egl.h>
#include <cstring>
#include <string>

namespace anxiety::rendering::backend::gles {
    // Constructor / destructor -------------------------------------------------------------------
    GLESDevice::GLESDevice(bool enable_validation) : GLDevice(DeferGLAD{}), m_enable_validation(enable_validation) {
        // El contexto de EGL todavía no está disponible — lo crea GLESSwapchain.
        // init_after_context() debe llamarse una vez que un contexto esté activo.
    }
    GLESDevice::~GLESDevice() = default;

    void GLESDevice::init_after_context() {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(eglGetProcAddress))) {
            LOG_ERROR("GLESDevice", "gladLoadGLLoader(eglGetProcAddress) falló.");
            return;
        }
        init_glad(m_enable_validation);
        LOG_INFO("GLESDevice", "Backend OpenGL ES activo.");
    }

    // compile_shader_from_source -----------------------------------------------------------------
    // GLES 3.x exige "#version 300 es" en la primera línea de cada shader, además de un
    // calificador de precisión explícito para float (los shaders de fragmento no tienen uno
    // por defecto en GLSL ES). Si el source de quien llama ya empieza con una directiva
    // "#version", se usa tal cual; en caso contrario se antepone el encabezado requerido.
    std::vector<uint8_t> GLESDevice::compile_shader_from_source(const char* source,const char* /*entry_point*/, rhi::ShaderStage  stage) {
        if (!source || source[0] == '\0') {
            LOG_ERROR("GLESDevice", "compile_shader_from_source: source vacío.");
            return {};
        }

        std::string patched;

        const bool has_version = (std::strncmp(source, "#version", 8) == 0);
        if (!has_version) {
            patched  = "#version 300 es\n";
            patched += "precision highp float;\n";
            patched += "precision highp int;\n";
            patched += source;
        } else {
            patched = source;
        }

        (void)stage; // la precisión específica por stage ya se maneja arriba (highp para todos)

        return std::vector<uint8_t>(patched.begin(), patched.end());
    }

    // create_swapchain ---------------------------------------------------------------------------
    std::unique_ptr<rhi::ISwapchain> GLESDevice::create_swapchain(const rhi::SwapchainDesc& desc) {
        auto swapchain = std::make_unique<GLESSwapchain>(desc);
        if (swapchain->is_valid() && !m_valid) {
            // El contexto de EGL ya está actual — termina la inicialización de GLAD.
            init_after_context();
        }
        return swapchain;
    }
} // namespace anxiety::rendering::backend::gles

#endif // ANXIETY_BACKEND_GLES
