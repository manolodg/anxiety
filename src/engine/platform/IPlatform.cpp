#include "IPlatform.h"

#include <cassert>

namespace anxiety::platform {
    // Singleton con el ciclo de vida del módulo — se establece en PlatformModule::on_init(), se
    // limpia en PlatformModule::on_shutdown().
    static IPlatform* s_current = nullptr;

    IPlatform& IPlatform::current() {
        assert(s_current != nullptr && "IPlatform::current() se llamó antes de PlatformModule::on_init().");
        return *s_current;
    }

    void IPlatform::set_current(IPlatform* platform) noexcept {
        s_current = platform;
    }
} // namespace anxiety::platform