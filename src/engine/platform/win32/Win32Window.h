#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "IWindow.h"

namespace anxiety::platform {
    // Win32Window --------------------------------------------------------------------------------
    // IWindow implementada con un HWND de Win32.
    //
    // El WNDCLASSEX se registra una vez por proceso (protegido por un flag estático). lpCreateParams
    // se usa para vincular cada HWND con su Win32Window* propietario, de modo que el procedimiento
    // de ventana pueda despachar mensajes sin estado global.
    // --------------------------------------------------------------------------------------------
    class Win32Window final : public IWindow {
    public:
        explicit Win32Window(const IWindow::Desc& desc);
        ~Win32Window() override;

        [[nodiscard]] bool     poll_events()                  override;
        void                   close()                        override;
        [[nodiscard]] bool     is_open()       const noexcept override;
        [[nodiscard]] uint32_t width()         const noexcept override;
        [[nodiscard]] uint32_t height()        const noexcept override;
        [[nodiscard]] void*    native_handle() const noexcept override;

    private:
        static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND     m_hwnd   { nullptr };
        uint32_t m_width  { 0 };
        uint32_t m_height { 0 };
        bool     m_open   { false };
    };
} // namespace anxiety::platform
