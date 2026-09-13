#include "Win32Platform.h"
#include "Win32Window.h"

namespace anxiety::platform {
    Win32Platform::Win32Platform()  = default;
    Win32Platform::~Win32Platform() = default;

    std::unique_ptr<IWindow> Win32Platform::create_window(const IWindow::Desc& desc) { return std::make_unique<Win32Window>(desc); }

    void Win32Platform::sleep_ms(uint32_t milliseconds) noexcept { ::Sleep(milliseconds); }
} // namespace anxiety::platform