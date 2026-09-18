#include "RpiPlatform.h"
#include "DrmKmsWindow.h"
#include "RpiTimer.h"
#include "RpiThread.h"
#include "Logger.h"

// Alternativa X11 — solo se incluye si DRM no está disponible
#include "../linux/LinuxWindow.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

namespace anxiety::platform {
    static constexpr std::string_view k_category = "RpiPlatform";

    // Constructor --------------------------------------------------------------------------------
    RpiPlatform::RpiPlatform() {
        // Prefiere DRM/KMS a menos que la sesión ya tenga una pantalla Wayland/X11.
        const bool has_display = (::getenv("DISPLAY") != nullptr) || (::getenv("WAYLAND_DISPLAY") != nullptr);
        // Comprobación rápida de disponibilidad de DRM
        const bool has_drm = (::access("/dev/dri/card0", F_OK) == 0);

        m_using_drm = has_drm && !has_display;
        LOGF_INFO(k_category, "Backend: {}.", name());
    }

    RpiPlatform::~RpiPlatform() = default;

    // IPlatform factory --------------------------------------------------------------------------
    std::unique_ptr<IWindow> RpiPlatform::create_window(const IWindow::Desc& desc) {
        if (m_using_drm) {
            auto win = std::make_unique<DrmKmsWindow>(desc);
            if (win->valid()) return win;
            LOG_WARN(k_category, "La creación de la ventana DRM/KMS falló — recurriendo a X11.");
            m_using_drm = false;
        }
        return std::make_unique<LinuxWindow>(desc);
    }

    std::unique_ptr<ITimer> RpiPlatform::create_timer() { return std::make_unique<RpiTimer>(); }

    std::unique_ptr<IThread> RpiPlatform::create_thread(IThread::Fn fn) { return std::make_unique<RpiThread>(std::move(fn)); }

    void RpiPlatform::sleep_ms(uint32_t ms) noexcept { ::usleep(static_cast<useconds_t>(ms) * 1000u); }
} // namespace anxiety::platform
