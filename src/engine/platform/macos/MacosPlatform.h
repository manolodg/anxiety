#pragma once

#include "IPlatform.h"

namespace anxiety::platform {
    // MacosPlatform ------------------------------------------------------------------------------
    // IPlatform backend for macOS (Cocoa, 10.15+).
    //
    // Extended services:
    //   • Display enumeration  ([NSScreen screens])
    //   • DPI                  ([NSWindow backingScaleFactor])
    //   • Window mode          (NSWindowStyleMaskFullScreen toggle)
    //   • Filesystem           (_NSGetExecutablePath, NSSearchPathForDirectoriesInDomains)
    //   • System info          (sysctl, IOKit for GPU name)
    //   • Clipboard            (NSPasteboard)
    // --------------------------------------------------------------------------------------------
    class MacosPlatform final : public IPlatform {
    public:
        MacosPlatform();
        ~MacosPlatform() override;

        [[nodiscard]] std::string_view name() const noexcept override { return "macOS"; }

        // IPlatform factory ----------------------------------------------------------------------
        [[nodiscard]] std::unique_ptr<IWindow> create_window(const IWindow::Desc& desc) override;

        void sleep_ms(uint32_t milliseconds) noexcept override;
    };
} // namespace anxiety::platform
