#pragma once

// NOTA: Esta cabecera incluye <windows.h>.
//       Inclúyela únicamente desde unidades de compilación de engine/platform/win32/
//       o desde PlatformModule.cpp (protegido por el guard ANXIETY_PLATFORM_WINDOWS).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "IPlatform.h"

namespace anxiety::platform {
    // Win32Platform ------------------------------------------------------------------------------
    // IPlatform concreta para el sistema operativo Windows. Crea instancias de Win32Window,
    // Win32Timer y Win32Thread.
    // --------------------------------------------------------------------------------------------
    class Win32Platform final : public IPlatform {
    public:
        Win32Platform();
        ~Win32Platform() override;

        [[nodiscard]] std::string_view name() const noexcept override { return "Win32"; }

        [[nodiscard]] std::unique_ptr<IWindow> create_window(const IWindow::Desc& desc) override;

        void sleep_ms(uint32_t milliseconds) noexcept override;
    };
} // namespace anxiety::platform