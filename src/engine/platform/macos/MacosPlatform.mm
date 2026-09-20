#include "MacosPlatform.h"
#include "MacosWindow.h"
#include "Logger.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <unistd.h>

namespace anxiety::platform {
    // Constructor / Destructor -------------------------------------------------------------------
    MacosPlatform::MacosPlatform() {
        @autoreleasepool {
            // Ensure NSApplication exists — needed for pollEvents() and window creation.
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
            [NSApp activateIgnoringOtherApps:YES];
        }
    }
    MacosPlatform::~MacosPlatform() = default;

    // IPlatform factory --------------------------------------------------------------------------
    std::unique_ptr<IWindow> MacosPlatform::create_window(const IWindow::Desc& desc) { return std::make_unique<MacosWindow>(desc); }

    void MacosPlatform::sleep_ms(uint32_t ms) noexcept { ::usleep(static_cast<useconds_t>(ms) * 1000u); }
} // namespace anxiety::platform
