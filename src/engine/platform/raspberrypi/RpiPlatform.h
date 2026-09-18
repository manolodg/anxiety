#pragma once

#include "IPlatform.h"

namespace anxiety::platform {
    // RpiPlatform --------------------------------------------------------------------------------
    // Backend de IPlatformSystem para Raspberry Pi ejecutando Linux sin escritorio.
    //
    // Estrategia de creación de ventana (en orden):
    //   1. DRM/KMS vía libdrm + libgbm  — headless, acceso directo a la GPU
    //   2. Alternativa X11 (LinuxWindow) — cuando la variable de entorno DISPLAY está definida
    //
    // Timer / Thread reutilizan las mismas implementaciones POSIX que LinuxPlatform. La información
    // del sistema y los helpers de hilos se comparten con LinuxPlatform.
    //
    // Requiere: libdrm-dev, libgbm-dev  (instalar vía apt en Raspberry Pi OS)
    // --------------------------------------------------------------------------------------------
    class RpiPlatform final : public IPlatform {
    public:
        RpiPlatform();
        ~RpiPlatform() override;

        [[nodiscard]] std::string_view name() const noexcept override { return m_using_drm ? "RaspberryPi/DRM" : "RaspberryPi/X11"; }

        // IPlatform factory ----------------------------------------------------------------------
        [[nodiscard]] std::unique_ptr<IWindow> create_window(const IWindow::Desc& desc) override;

        void sleep_ms(uint32_t milliseconds) noexcept override;

    private:
        bool m_using_drm{ false };
    };
} // namespace anxiety::platform
