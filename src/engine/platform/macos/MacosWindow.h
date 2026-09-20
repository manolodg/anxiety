#pragma once

#include "IWindow.h"

// Forward-declare ObjC objects so this header stays clean C++.
#ifdef __OBJC__
@class NSWindow;
@class AnxietyView;
#else
using NSWindow    = struct objc_object;
using AnxietyView = struct objc_object;
#endif

namespace anxiety::platform {
    // MacosWindow --------------------------------------------------------------------------------
    // IWindow backed by an NSWindow + custom NSView.
    //
    // NSApplication must be initialised before creating the first window; this is done in
    // MacosPlatform constructor.
    //
    // poll_events() drains the NSApplication event queue via
    //   -[NSApp nextEventMatchingMask:untilDate:inMode:dequeue:]
    // with distantPast so it never blocks.
    // --------------------------------------------------------------------------------------------
    class MacosWindow final : public IWindow {
    public:
        explicit MacosWindow(const IWindow::Desc& desc);
        ~MacosWindow() override;

        [[nodiscard]] bool     poll_events()                  override;
        void                   close()                        override;
        [[nodiscard]] bool     is_open()       const noexcept override;
        [[nodiscard]] uint32_t width()         const noexcept override;
        [[nodiscard]] uint32_t height()        const noexcept override;
        [[nodiscard]] void*    native_handle() const noexcept override;

        // Called by the ObjC delegate / view when events arrive.
        void on_closed();

    private:
        NSWindow*     m_window { nullptr };
        AnxietyView*  m_view   { nullptr };
        uint32_t      m_width  { 0 };
        uint32_t      m_height { 0 };
        bool          m_open   { false };
    };

} // namespace anxiety::platform
