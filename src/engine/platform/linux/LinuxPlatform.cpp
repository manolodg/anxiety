#include "LinuxPlatform.h"
#include "LinuxWindow.h"

#if defined(ANXIETY_HAS_WAYLAND)
#include "WaylandWindow.h"
#endif

#include <chrono>
#include <cstdlib>
#include <thread>

namespace anxiety::platform {
    LinuxPlatform::LinuxPlatform()  = default;
    LinuxPlatform::~LinuxPlatform() = default;

    std::unique_ptr<IWindow> LinuxPlatform::create_window(const IWindow::Desc& desc) {
#if defined(ANXIETY_HAS_WAYLAND)
        // Preferir Wayland cuando hay un compositor disponible (WAYLAND_DISPLAY seteado);
        // en caso contrario, o si no hay soporte compilado, usar X11.
        if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
            return std::make_unique<WaylandWindow>(desc);
        }
#endif
        return std::make_unique<LinuxWindow>(desc);
    }

    void LinuxPlatform::sleep_ms(uint32_t milliseconds) noexcept { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
} // namespace anxiety::platform
