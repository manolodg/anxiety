#pragma once

#include <cstdint>
#include <string>

namespace anxiety::platform {
    // IWindow ------------------------------------------------------------------------------------
    // Handle abstracto a una ventana del SO. Creado por IPlatform::create_window().
    //
    // Thread safety:
    //   poll_events() debe llamarse desde el hilo que creó la ventana.
    // --------------------------------------------------------------------------------------------
    class IWindow {
    public:
        virtual ~IWindow() = default;

        // Descriptor de creación ------------------------------------------------------------------
        struct Desc {
            std::string title     = "Anxiety";
            uint32_t    width     = 1280;
            uint32_t    height    = 720;
            bool        resizable = true;
        };

        // Ciclo de vida ----------------------------------------------------------------------------
        // Procesa los mensajes de SO pendientes. Devuelve false cuando la ventana se ha cerrado
        // (WM_CLOSE o equivalente). Debe llamarse desde el hilo propietario de la ventana.
        [[nodiscard]] virtual bool poll_events() = 0;
        // Solicita un cierre programático (equivalente a que el usuario pulse la x).
        virtual void               close()       = 0;

        // Consultas --------------------------------------------------------------------------------
        [[nodiscard]] virtual bool     is_open()       const noexcept = 0;
        [[nodiscard]] virtual uint32_t width()         const noexcept = 0;
        [[nodiscard]] virtual uint32_t height()        const noexcept = 0;

        // Handle opaco al objeto de SO subyacente.
        //  Win32 -> HWND. Convertir (cast) al tipo esperado donde sea necesario.
        [[nodiscard]] virtual void*    native_handle() const noexcept = 0;

    protected:
        IWindow() = default;

        IWindow(const IWindow&)            = delete;
        IWindow& operator=(const IWindow&) = delete;
    };
} // namespace anxiety::platform