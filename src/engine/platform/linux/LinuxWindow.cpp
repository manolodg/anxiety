#include "LinuxWindow.h"
#include "Logger.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace anxiety::platform {
    static constexpr std::string_view k_category = "LinuxWindow";

    namespace {
        // primary_monitor_scale ------------------------------------------------------------------
        // GNOME/Mutter draws legacy X11 (Xwayland) clients at their literal requested pixel count,
        // then leaves it up to the app to look "the right size" — unlike scale-aware Wayland/GTK
        // apps, which render at physical resolution for a given logical size. On a monitor with a
        // fractional scale set (e.g. 1.25), a plain Xlib window asking for 1280x720 therefore looks
        // smaller on screen than a scale-aware app's "1280x720" would.
        //
        // There is no X11 protocol mechanism to query this (RandR doesn't expose Mutter's
        // fractional scale), so it's read directly from GNOME's monitors.xml, which mirrors the
        // live display configuration. Falls back to 1.0 (no adjustment) on any non-GNOME desktop
        // or if the file is missing/unparsable.
        double primary_monitor_scale() {
            const char* home = std::getenv("HOME");
            if (!home) return 1.0;

            std::ifstream file(std::string(home) + "/.config/monitors.xml");
            if (!file) return 1.0;

            const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            size_t pos = 0;
            while (true) {
                const size_t start = content.find("<logicalmonitor>", pos);
                if (start == std::string::npos) break;
                const size_t end = content.find("</logicalmonitor>", start);
                if (end == std::string::npos) break;

                const std::string block = content.substr(start, end - start);
                pos = end + 1;

                if (block.find("<primary>yes</primary>") == std::string::npos) continue;

                const size_t scale_start = block.find("<scale>");
                const size_t scale_end   = block.find("</scale>");
                if (scale_start == std::string::npos || scale_end == std::string::npos) break;

                try {
                    return std::stod(block.substr(scale_start + std::strlen("<scale>"), scale_end - scale_start));
                } catch (...) {
                    return 1.0;
                }
            }
            return 1.0;
        }
    } // namespace

    // Constructor / Destructor -------------------------------------------------------------------
    LinuxWindow::LinuxWindow(const IWindow::Desc& desc) : m_width (desc.width), m_height(desc.height) {
        // Pre-scale the requested size by the primary monitor's fractional scale so the window's
        // on-screen footprint matches what the caller asked for (see primary_monitor_scale() above).
        const double   scale  = primary_monitor_scale();
        const uint32_t width  = static_cast<uint32_t>(std::lround(desc.width  * scale));
        const uint32_t height = static_cast<uint32_t>(std::lround(desc.height * scale));
        m_width  = width;
        m_height = height;

        // Abre conexión al servidor X ------------------------------------------------------------
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            LOG_ERROR(k_category, "XOpenDisplay() fallido. Esta $DISPLAY configurado? Esta un X server en ejecución?");
            return;
        }

        int    screen = DefaultScreen(m_display);
        Window root   = RootWindow(m_display, screen);

        // Crea la ventana ------------------------------------------------------------------------
        m_window = XCreateSimpleWindow(
            m_display, root,
            0, 0,
            width, height,
            0,                                      // ancho del borde
            BlackPixel(m_display, screen),          // color del borde
            WhitePixel(m_display, screen));         // color de fondo

        if (!m_window) {
            LOG_ERROR(k_category, "XCreateSimpleWindow() fallido.");
            XCloseDisplay(m_display);
            m_display = nullptr;
            return;
        }

        // Título de Window -----------------------------------------------------------------------
        // XStoreName fija WM_NAME como STRING (Latin-1) — cualquier título con caracteres fuera de
        // ese rango (p.ej. una raya "—") sale corrupto. Los WM modernos (GNOME/KDE/XFCE) prefieren
        // la propiedad EWMH _NET_WM_NAME en UTF8_STRING; XStoreName se deja solo como fallback.
        XStoreName(m_display, m_window, desc.title.c_str());

        const Atom utf8_string  = XInternAtom(m_display, "UTF8_STRING", False);
        const Atom net_wm_name  = XInternAtom(m_display, "_NET_WM_NAME", False);
        XChangeProperty(m_display, m_window, net_wm_name, utf8_string, 8, PropModeReplace,
                         reinterpret_cast<const unsigned char*>(desc.title.data()), static_cast<int>(desc.title.size()));

        // Configura el ancho de hints (honour resizable flag) ------------------------------------
        if (!desc.resizable) {
            XSizeHints* hints = XAllocSizeHints();
            if (hints) {
                hints->flags  = PSize;
                hints->width  = static_cast<int>(width);
                hints->height = static_cast<int>(height);

                if (!desc.resizable) {
                    hints->flags      = PMinSize | PMaxSize;
                    hints->min_width  = static_cast<int>(width);
                    hints->min_height = static_cast<int>(height);
                    hints->max_width  = static_cast<int>(width);
                    hints->max_height = static_cast<int>(height);
                }

                XSetWMNormalHints(m_display, m_window, hints);
                XFree(hints);
            }
        }

        // Suscribe los eventos -------------------------------------------------------------------
        XSelectInput(m_display, m_window, ExposureMask | StructureNotifyMask);

        // Intercepta WM_DELETE_WINDOW (botón de cierre) ------------------------------------------
        m_wm_delete_window = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(m_display, m_window, &m_wm_delete_window, 1);

        // Muestra la ventana ---------------------------------------------------------------------
        XMapWindow(m_display, m_window);
        XFlush(m_display);

        m_open = true;
        LOGF_INFO(k_category, "X11 window '{}' creada ({}x{}, id=0x{:X}, escala de monitor={:.2f}).", desc.title, m_width, m_height, static_cast<unsigned long>(m_window), scale);
    }

    LinuxWindow::~LinuxWindow() {
        if (m_display) {
            if (m_window) {
                XDestroyWindow(m_display, m_window);
                m_window = 0;
            }

            XCloseDisplay(m_display);
            m_display = nullptr;
        }
    }

    // IWindow ------------------------------------------------------------------------------------
    bool LinuxWindow::poll_events() {
        if (!m_display || !m_open) return false;

        XEvent ev;
        while (XPending(m_display) > 0) {
            XNextEvent(m_display, &ev);

            switch (ev.type) {

            case ClientMessage:
                // WM_DELETE_WINDOW: el usuario ha pulsado el botón X
                if (static_cast<Atom>(ev.xclient.data.l[0]) == m_wm_delete_window) {
                    m_open = false;
                    return false;
                }
                break;

            case ConfigureNotify: {
                // Lanzado en reescalado o movimiento; solo actualiza las dimensiones cuando cambian
                const auto w = static_cast<uint32_t>(ev.xconfigure.width);
                const auto h = static_cast<uint32_t>(ev.xconfigure.height);
                if (w != m_width || h != m_height) {
                    m_width  = w;
                    m_height = h;
                }
                break;
            }

            case DestroyNotify:
                // WM ha forzado la destrucción de la ventana
                m_open   = false;
                m_window = 0;   // ya se ha ido - no llamar XDestroyWindow en el destructor
                return false;

            case Expose:
                // Solicitud de redibujado — el render de la GPU maneja esto
                break;

            default:
                break;
            }
        }

        return m_open;
    }

    void LinuxWindow::close() {
        // Sintetiza el mensaje WM_DELETE_WINDOW por nosotros mismos para que el
        // flujo siga la misma ruta que un cierre de usuario.
        if (!m_display || !m_window) return;

        XEvent ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.type                 = ClientMessage;
        ev.xclient.window       = m_window;
        ev.xclient.message_type = XInternAtom(m_display, "WM_PROTOCOLS", False);
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = static_cast<long>(m_wm_delete_window);
        ev.xclient.data.l[1]    = CurrentTime;

        XSendEvent(m_display, m_window, False, NoEventMask, &ev);
        XFlush(m_display);
    }

    bool     LinuxWindow::is_open()       const noexcept { return m_open;    }
    uint32_t LinuxWindow::width()         const noexcept { return m_width;   }
    uint32_t LinuxWindow::height()        const noexcept { return m_height;  }

    void* LinuxWindow::native_handle() const noexcept {
        // Las ventana es tipada a unsigned long; haz cast mediante via uintptr_t para corregir
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_window));
    }
} // namespace anxiety::platform
