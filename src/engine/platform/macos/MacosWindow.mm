// Objective-C++ — compiled only on macOS (LANGUAGES ObjCXX enabled in CMake)
#include "MacosWindow.h"
#include "Logger.h"

#import <Cocoa/Cocoa.h>
#include <cstring>

namespace ep = anxiety::platform;

static constexpr std::string_view k_category = "MacosWindow";

// ObjC View - receives Cocoa events --------------------------------------------------------------

@interface AnxietyView : NSView
@property (nonatomic, assign) ep::MacosWindow* cppWindow;
@end

@implementation AnxietyView

- (BOOL)acceptsFirstResponder { return YES; }

@end

// Window delegate — detects close ----------------------------------------------------------------

@interface AnxietyWindowDelegate : NSObject<NSWindowDelegate>
@property (nonatomic, assign) ep::MacosWindow* cppWindow;
@end

@implementation AnxietyWindowDelegate
- (BOOL)windowShouldClose:(id)sender { (void)sender; return YES; }
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    self.cppWindow->on_closed();
}
@end

// MacosWindow ------------------------------------------------------------------------------------

namespace anxiety::platform {
    MacosWindow::MacosWindow(const IWindow::Desc& desc) : m_width(desc.width), m_height(desc.height) {
        @autoreleasepool {
            NSRect            frame = NSMakeRect(100, 100, static_cast<CGFloat>(desc.width), static_cast<CGFloat>(desc.height));
            NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;

            m_window = [[NSWindow alloc]
                initWithContentRect:frame
                          styleMask:style
                            backing:NSBackingStoreBuffered
                              defer:NO];

            NSString* title = [NSString stringWithUTF8String:desc.title.c_str()];
            [m_window setTitle:title];
            [m_window setAcceptsMouseMovedEvents:YES];

            // View
            m_view = [[AnxietyView alloc] initWithFrame:frame];
            m_view.cppWindow = this;
            [m_window setContentView:m_view];
            [m_window makeFirstResponder:m_view];

            // Delegate
            AnxietyWindowDelegate* del = [[AnxietyWindowDelegate alloc] init];
            del.cppWindow = this;
            [m_window setDelegate:del];

            [m_window makeKeyAndOrderFront:nil];
        }
        m_open = true;
        LOGF_INFO(k_category, "NSWindow '{}' created {}x{}.", desc.title, m_width, m_height);
    }
    MacosWindow::~MacosWindow() {
        @autoreleasepool {
            if (m_window) {
                [m_window setDelegate:nil];
                [m_window close];
                m_window = nullptr;
            }
        }
    }

    bool MacosWindow::poll_events() {
        @autoreleasepool {
            NSEvent* event;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES])) {
                [NSApp sendEvent:event];
            }
        }
        return m_open;
    }

    void MacosWindow::close() {
        @autoreleasepool {
            if (m_window) [m_window performClose:nil];
        }
    }

    bool     MacosWindow::is_open()       const noexcept { return m_open;   }
    uint32_t MacosWindow::width()         const noexcept { return m_width;  }
    uint32_t MacosWindow::height()        const noexcept { return m_height; }
    // Se entrega la vista, no la ventana: los backends de rendering (GL/Vulkan/Metal) dibujan
    // directamente sobre una NSView — el mismo contrato que usa el modo "ventana embebida"
    // (attach_window), de modo que ambos modos comparten idéntico código de creación de contexto.
    void*    MacosWindow::native_handle() const noexcept { return m_view; }

    void MacosWindow::on_closed() { m_open = false; }
} // namespace anxiety::platform
