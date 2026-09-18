#pragma once

#include "IWindow.h"
#include "InputTypes.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

// libdrm / libgbm — solo presentes cuando IAENGINE_HAS_DRM está definido (lo establece CMake)
struct gbm_device;
struct gbm_surface;

// Declaración anticipada de la información de modo DRM (de xf86drmMode.h) sin incluir el
// header completo en esta interfaz pública.
struct _drmModeModeInfo;

namespace anxiety::platform {
    // DrmKmsNativeHandle -------------------------------------------------------------------------
    // Expuesto vía nativeHandle(). El renderer (ruta EGL) lo convierte a este struct para obtener
    // la superficie GBM requerida por eglCreateWindowSurface().
    // --------------------------------------------------------------------------------------------
    struct DrmKmsNativeHandle {
        // Etiqueta mágica usada por el backend de Vulkan para distinguir este struct de un ID de
        // ventana X11 almacenado como void*. En Linux de 64 bits, los punteros de heap superan los
        // 4 GB mientras que los ID de X11 son de 32 bits (≤ 0xFFFFFFFF), así que el backend primero
        // comprueba el rango de direcciones y luego verifica k_tag antes de tocar cualquier otro campo.
        static constexpr uint32_t k_tag = 0x44524D4Bu; // 'DRMK'
        uint32_t     type_tag{ k_tag };
        int          drm_fd{ -1 };            // descriptor de archivo abierto hacia /dev/dri/cardN
        gbm_device* gbm_dev{ nullptr };       // device GBM que envuelve drmFd
        gbm_surface* gbm_surf{ nullptr };       // superficie GBM (destino de ventana EGL)
        uint32_t     width{ 0 };
        uint32_t     height{ 0 };
        uint32_t     connector_id{ 0 };             // ID de conector DRM (para VK_EXT_acquire_drm_display de Vulkan)
    };

    // DrmKmsWindow -------------------------------------------------------------------------------
    // IWindow respaldada por el subsistema Linux DRM/KMS + GBM. Orientada a Raspberry Pi 4/5
    // ejecutándose sin servidor de pantalla.
    //
    // native_handle() devuelve un puntero a un DrmKmsNativeHandle interno que el renderer EGL usa
    // para crear su superficie de ventana.
    //
    // Entrada: no disponible sin un subsistema de entrada aparte (evdev / libinput). Esta
    //          implementación deja la cola de entrada vacía; integra libinput por separado y llama
    //          a push_input_event() desde su callback.
    // --------------------------------------------------------------------------------------------
    class DrmKmsWindow final : public IWindow {
    public:
        explicit DrmKmsWindow(const IWindow::Desc& desc);
        ~DrmKmsWindow() override;

        [[nodiscard]] bool     poll_events()               override;
        void                   close()                    override;
        [[nodiscard]] bool     is_open()     const noexcept override;
        [[nodiscard]] uint32_t width()      const noexcept override;
        [[nodiscard]] uint32_t height()     const noexcept override;
        [[nodiscard]] void* native_handle() const noexcept override;

        [[nodiscard]] bool valid()      const noexcept { return m_handle.drm_fd >= 0; }

    private:
        DrmKmsNativeHandle     m_handle;
        bool                   m_open{ false };

        bool open_device(const char* path);
        bool find_connector_and_mode(const IWindow::Desc& desc);
    };
} // namespace anxiety::platform
