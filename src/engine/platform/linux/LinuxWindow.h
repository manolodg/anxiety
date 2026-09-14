#pragma once

// NOTE: Esta cabecera incluye X11/Xlib.h.
//       Solo lo incluye dentro de las unidades de compilación engine/platform/linux/ o desde
//       PlatformModule.cpp (tras ANXIETY_PLATFORM_LINUX).

#include "IWindow.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace anxiety::platform {
    // LinuxWindow --------------------------------------------------------------------------------
    // IWindow implementado sobre Xlib.
    //
    // Cada instancia posee una conexión Display y un Window. poll_events() drena los eventos X de
    // la cola sin bloquear - usa XPending() para comprobar eventos pendientes antes de llamar
    // XNextEvent().
    //
    // Handled events:
    //   ClientMessage / WM_DELETE_WINDOW — usuario ha pulsado X, marca ventana cerrada
    //   ConfigureNotify                  — ventana reescalada / movida, actualiza dimensiones
    //   DestroyNotify                    — WM destruye la ventana externamente
    //   Expose                           — consumido silenciosamente (la GPU maneja el redibujado)
    // --------------------------------------------------------------------------------------------
    class LinuxWindow final : public IWindow {
    public:
        explicit LinuxWindow(const IWindow::Desc& desc);
        ~LinuxWindow() override;

        [[nodiscard]] bool     poll_events()                  override;
        void                   close()                        override;
        [[nodiscard]] bool     is_open()       const noexcept override;
        [[nodiscard]] uint32_t width()         const noexcept override;
        [[nodiscard]] uint32_t height()        const noexcept override;
        [[nodiscard]] void*    native_handle() const noexcept override;

    private:
        Display* m_display            { nullptr };
        Window   m_window             { 0 };
        Atom     m_wm_delete_window   { 0 };
        uint32_t m_width              { 0 };
        uint32_t m_height             { 0 };
        bool     m_open               { false };
    };
} // namespace anxiety::platform
