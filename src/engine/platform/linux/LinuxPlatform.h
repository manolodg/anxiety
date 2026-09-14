#pragma once

// NOTE: Esta cabecera NO incluye cabeceras X11.
//       X11 solo se mete en LinuxWindow.h / LinuxWindow.cpp.

#include "IPlatform.h"

#include <cstdlib>

namespace anxiety::platform {
    // LinuxPlatform ------------------------------------------------------------------------------
    // IPlatform concreto para Linux / X11.
    // Actua como una factoría; crea LinuxWindow.
    // --------------------------------------------------------------------------------------------
    class LinuxPlatform final : public IPlatform {
    public:
        LinuxPlatform();
        ~LinuxPlatform() override;

        [[nodiscard]] std::string_view name() const noexcept override {
#if defined(ANXIETY_HAS_WAYLAND)
            if (std::getenv("WAYLAND_DISPLAY") != nullptr) return "Linux/Wayland";
#endif
            return "Linux/X11";
        }

        [[nodiscard]] std::unique_ptr<IWindow> create_window(const IWindow::Desc& desc) override;

        void sleep_ms(uint32_t milliseconds) noexcept override;
    };
} // namespace anxiety::platform
