#include "RHIFactory.h"
#include "Logger.h"

#include <cstring>
#include <cctype>
#include <algorithm>
#include <string>

// Cabeceras de backend (protegidas por defines en tiempo de compilación) --------------------------
#if defined(_WIN32)
#include "backend/dx12/DX12Device.h"
#endif

#if ANXIETY_BACKEND_DX11
#include "backend/dx11/DX11Device.h"
#endif

#if ANXIETY_BACKEND_VULKAN
#include "backend/vulkan/VulkanDevice.h"
#endif

#if ANXIETY_BACKEND_METAL
#include "backend/metal/MetalDevice.h"
#endif

#if ANXIETY_BACKEND_OPENGL
#include "backend/opengl/GLDevice.h"
#endif

#if ANXIETY_BACKEND_GLES
#include "backend/gles/GLESDevice.h"
#endif

namespace anxiety::rendering::rhi {
    static constexpr std::string_view k_category = "RHIFactory";

    // parse_command_line -------------------------------------------------------------------------
    RHIBackend RHIFactory::parse_command_line(int argc, const char* const* argv) noexcept {
        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];
            if (std::strncmp(arg, "--rhi=", 6) == 0) {
                std::string val = arg + 6;
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                if (val == "dx12" || val == "d3d12")  return RHIBackend::DirectX12;
                if (val == "dx11" || val == "d3d11")  return RHIBackend::DirectX11;
                if (val == "vulkan")                  return RHIBackend::Vulkan;
                if (val == "metal")                   return RHIBackend::Metal;
                if (val == "opengl" || val == "gl")   return RHIBackend::OpenGL;
                if (val == "gles" || val == "gles3")  return RHIBackend::OpenGLES;
            }
        }
        return RHIBackend::Unknown;
    }

    // select_backend -----------------------------------------------------------------------------
    RHIBackend RHIFactory::select_backend(RHIBackend preferred) noexcept {
        // Respeta el override explícito si ese backend está compilado.
        if (preferred != RHIBackend::Unknown) {
            switch (preferred) {
#if defined(_WIN32)
            case RHIBackend::DirectX12:    return RHIBackend::DirectX12;
#endif
#ifdef ANXIETY_BACKEND_DX11
            case RHIBackend::DirectX11:    return RHIBackend::DirectX11;
#endif
#ifdef ANXIETY_BACKEND_VULKAN
            case RHIBackend::Vulkan:       return RHIBackend::Vulkan;
#endif
#ifdef ANXIETY_BACKEND_METAL
            case RHIBackend::Metal:        return RHIBackend::Metal;
#endif
#ifdef ANXIETY_BACKEND_OPENGL
            case RHIBackend::OpenGL:       return RHIBackend::OpenGL;
#endif
#ifdef ANXIETY_BACKEND_GLES
            case RHIBackend::OpenGLES:     return RHIBackend::OpenGLES;
#endif
            default:
                LOGF_WARNING(k_category, "El backend solicitado '{}' no está compilado; se recurre a la selección automática.", backend_name(preferred));
                break;
            }
        }

        // Selección automática según la prioridad de la plataforma.
#if defined(_WIN32)
        // Prioridad en Win32: DX12
        return RHIBackend::DirectX12;
#elif defined(__APPLE__)
#  ifdef ANXIETY_BACKEND_METAL
        return RHIBackend::Metal;
#  elif defined(ANXIETY_BACKEND_OPENGL)
        return RHIBackend::OpenGL;
#  else
        return RHIBackend::Unknown;
#  endif
#elif defined(ANXIETY_PLATFORM_RPI)
#  ifdef ANXIETY_BACKEND_VULKAN
        return RHIBackend::Vulkan;
#  elif defined(ANXIETY_BACKEND_GLES)
        return RHIBackend::OpenGLES;
#  else
        return RHIBackend::Unknown;
#  endif
#else // Linux / otros POSIX
#  ifdef ANXIETY_BACKEND_VULKAN
        return RHIBackend::Vulkan;
#  elif defined(ANXIETY_BACKEND_OPENGL)
        return RHIBackend::OpenGL;
#  else
        return RHIBackend::Unknown;
#  endif
#endif
    }

    // create_device ------------------------------------------------------------------------------
    std::unique_ptr<IDevice> RHIFactory::create_device(RHIBackend backend, bool enable_validation, [[maybe_unused]] void* native_window) {
        LOGF_INFO(k_category, "Creando dispositivo {} (validación={}).", backend_name(backend), enable_validation);

        switch (backend) {
#if defined(_WIN32)
        case RHIBackend::DirectX12: {
            auto dev = std::make_unique<backend::dx12::DX12Device>(enable_validation);
            if (!dev->is_valid()) {
                LOGF_ERROR(k_category, "La creación de DX12Device falló.");
                return nullptr;
            }
            return dev;
        }
#endif

#ifdef ANXIETY_BACKEND_DX11
        case RHIBackend::DirectX11: {
            auto dev = std::make_unique<backend::dx11::DX11Device>(enable_validation);
            if (!dev->is_valid()) {
                LOGF_ERROR(k_category, "La creación de DX11Device falló.");
                return nullptr;
            }
            return dev;
        }
#endif

#ifdef ANXIETY_BACKEND_VULKAN
        case RHIBackend::Vulkan: {
            auto dev = std::make_unique<backend::vulkan::VulkanDevice>(enable_validation);
            if (!dev->is_valid()) {
                LOGF_ERROR(k_category, "La creación de VulkanDevice falló.");
                return nullptr;
            }
            return dev;
        }
#endif

#ifdef ANXIETY_BACKEND_METAL
        case RHIBackend::Metal: {
            auto dev = std::make_unique<backend::metal::MetalDevice>(enable_validation);
            if (!dev->is_valid()) {
                LOGF_ERROR(k_category, "La creación de MetalDevice falló.");
                return nullptr;
            }
            return dev;
        }
#endif

#ifdef ANXIETY_BACKEND_OPENGL
        case RHIBackend::OpenGL: {
            auto dev = std::make_unique<backend::opengl::GLDevice>(enable_validation, native_window);
            if (!dev->is_valid()) {
                LOGF_ERROR(k_category, "La creación de GLDevice falló.");
                return nullptr;
            }
            return dev;
        }
#endif

#ifdef ANXIETY_BACKEND_GLES
        case RHIBackend::OpenGLES: {
            // GLESDevice aplaza la inicialización de GLAD hasta que create_swapchain() establece
            // el contexto EGL — is_valid() es deliberadamente falso aquí. No lo compruebes.
            return std::make_unique<backend::gles::GLESDevice>(enable_validation);
        }
#endif

        default:
            LOGF_ERROR(k_category, "El backend '{}' no está disponible en esta build.", backend_name(backend));
            return nullptr;
        }
    }

    // create_best_device -------------------------------------------------------------------------
    std::unique_ptr<IDevice> RHIFactory::create_best_device(int argc, const char* const* argv, bool enable_validation) {
        const RHIBackend preferred = parse_command_line(argc, argv);
        const RHIBackend selected  = select_backend(preferred);
        if (selected == RHIBackend::Unknown) {
            LOGF_ERROR(k_category, "No hay ningún backend de RHI disponible en esta plataforma/build.");
            return nullptr;
        }
        return create_device(selected, enable_validation);
    }
} // namespace anxiety::rendering::rhi