#include "WaylandWindow.h"
#include "Logger.h"

// Protocolo generado (xdg-shell-protocol.c compilado separandamente por CMake)
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace anxiety::platform {
    static constexpr std::string_view k_category = "WaylandWindow";

    // Tablas de escucha --------------------------------------------------------------------------

    const wl_registry_listener WaylandWindow::s_registryListener = {
        onRegistryGlobal,
        onRegistryGlobalRemove,
    };

    const xdg_wm_base_listener WaylandWindow::s_wmBaseListener = {
        onWmBasePing,
    };

    const xdg_surface_listener WaylandWindow::s_xdgSurfaceListener = {
        onXdgSurfaceConfigure,
    };

    const xdg_toplevel_listener WaylandWindow::s_toplevelListener = {
        onToplevelConfigure,
        onToplevelClose,
        onToplevelConfigureBounds,
        onToplevelWmCapabilities,
    };

    const wl_seat_listener WaylandWindow::s_seatListener = {
        onSeatCapabilities,
        onSeatName,
    };

    const wl_touch_listener WaylandWindow::s_touchListener = {
        onTouchDown,
        onTouchUp,
        onTouchMotion,
        onTouchFrame,
        onTouchCancel,
        onTouchShape,
        onTouchOrientation,
    };

    const wl_keyboard_listener WaylandWindow::s_keyboardListener = {
        onKeyboardKeymap,
        onKeyboardEnter,
        onKeyboardLeave,
        onKeyboardKey,
        onKeyboardModifiers,
        onKeyboardRepeatInfo,
    };

    const wl_pointer_listener WaylandWindow::s_pointerListener = {
        onPointerEnter,
        onPointerLeave,
        onPointerMotion,
        onPointerButton,
        onPointerAxis,
        onPointerFrame,
        onPointerAxisSource,
        onPointerAxisStop,
        onPointerAxisDiscrete,
    };

    const wl_buffer_listener WaylandWindow::s_bufferListener = {
        onBufferRelease,
    };

    // Callbacks de protocolo -----------------------------------------------------------------------
    namespace {
        constexpr uint32_t kCompositorVersion = 4;
        constexpr uint32_t kWmBaseVersion     = 5;
        constexpr uint32_t kSeatVersion       = 5;
    } // namespace

    void WaylandWindow::onRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
        auto* self = static_cast<WaylandWindow*>(data);

        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            self->m_compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, kCompositorVersion)));
        } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            // El listener se añade en el constructor, una vez confirmado que el global existe.
            self->m_wmBase = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, kWmBaseVersion)));
        } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            self->m_seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, kSeatVersion)));
            wl_seat_add_listener(self->m_seat, &s_seatListener, self);
        } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            self->m_shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        }
    }

    void WaylandWindow::onRegistryGlobalRemove(void* /*data*/, wl_registry* /*registry*/, uint32_t /*name*/) {
        // Los globals no se retiran dinámicamente en esta implementación mínima — la ventana no
        // sobrevive a la desaparición del compositor.
    }

    void WaylandWindow::onWmBasePing(void* /*data*/, xdg_wm_base* wmBase, uint32_t serial) {
        xdg_wm_base_pong(wmBase, serial);
    }

    void WaylandWindow::onXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial) {
        auto* self = static_cast<WaylandWindow*>(data);
        xdg_surface_ack_configure(surface, serial);
        self->m_configured = true;
        self->attachFrame();
    }

    void WaylandWindow::onToplevelConfigure(void* data, xdg_toplevel* /*toplevel*/, int32_t width, int32_t height, wl_array* /*states*/) {
        auto* self = static_cast<WaylandWindow*>(data);
        if (width > 0 && height > 0) {
            self->m_width  = static_cast<uint32_t>(width);
            self->m_height = static_cast<uint32_t>(height);
        }
    }

    void WaylandWindow::onToplevelClose(void* data, xdg_toplevel* /*toplevel*/) {
        static_cast<WaylandWindow*>(data)->m_open = false;
    }

    void WaylandWindow::onToplevelConfigureBounds(void* /*data*/, xdg_toplevel* /*toplevel*/, int32_t /*width*/, int32_t /*height*/) {
        // Informativo (límites recomendados del compositor) — no aplicable sin gestión de tamaño.
    }

    void WaylandWindow::onToplevelWmCapabilities(void* /*data*/, xdg_toplevel* /*toplevel*/, wl_array* /*capabilities*/) {
        // Informativo — no usado todavía.
    }

    void WaylandWindow::onSeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities) {
        auto* self = static_cast<WaylandWindow*>(data);

        const bool hasPointer  = (capabilities & WL_SEAT_CAPABILITY_POINTER)  != 0;
        const bool hasKeyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        const bool hasTouch    = (capabilities & WL_SEAT_CAPABILITY_TOUCH)    != 0;

        if (hasPointer && !self->m_pointer) {
            self->m_pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(self->m_pointer, &s_pointerListener, self);
        } else if (!hasPointer && self->m_pointer) {
            wl_pointer_destroy(self->m_pointer);
            self->m_pointer = nullptr;
        }

        if (hasKeyboard && !self->m_keyboard) {
            self->m_keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(self->m_keyboard, &s_keyboardListener, self);
        } else if (!hasKeyboard && self->m_keyboard) {
            wl_keyboard_destroy(self->m_keyboard);
            self->m_keyboard = nullptr;
        }

        if (hasTouch && !self->m_touch) {
            self->m_touch = wl_seat_get_touch(seat);
            wl_touch_add_listener(self->m_touch, &s_touchListener, self);
        } else if (!hasTouch && self->m_touch) {
            wl_touch_destroy(self->m_touch);
            self->m_touch = nullptr;
        }
    }

    void WaylandWindow::onSeatName(void* /*data*/, wl_seat* /*seat*/, const char* /*name*/) {}

    void WaylandWindow::onBufferRelease(void* /*data*/, wl_buffer* buffer) {
        // Buffer "de usar y tirar": el compositor ha terminado con él, ya se puede liberar.
        wl_buffer_destroy(buffer);
    }

    // Framebuffer (wl_shm) -------------------------------------------------------------------------
    namespace {
        // Crea un fichero anónimo en memoria (memfd) del tamaño dado, listo para mmap().
        int createSharedMemoryFile(size_t size) {
            const int fd = memfd_create("anxiety-wl-shm", MFD_CLOEXEC);
            if (fd < 0) return -1;
            if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
                ::close(fd);
                return -1;
            }
            return fd;
        }
    } // namespace

    void WaylandWindow::attachFrame() {
        if (!m_shm || !m_surface || m_width == 0 || m_height == 0) return;

        const int32_t stride = static_cast<int32_t>(m_width) * 4;
        const size_t  size   = static_cast<size_t>(stride) * m_height;

        const int fd = createSharedMemoryFile(size);
        if (fd < 0) {
            LOG_ERROR(k_category, "No se pudo crear el fichero de memoria compartida para el framebuffer.");
            return;
        }

        void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            LOG_ERROR(k_category, "mmap() fallido para el framebuffer Wayland.");
            ::close(fd);
            return;
        }

        // Placeholder: relleno de color sólido hasta que exista un backend de render real (EGL/Vulkan).
        constexpr uint32_t kFillColor = 0xFF202020; // XRGB8888
        std::fill_n(static_cast<uint32_t*>(data), static_cast<size_t>(m_width) * m_height, kFillColor);
        munmap(data, size);

        wl_shm_pool* pool   = wl_shm_create_pool(m_shm, fd, static_cast<int32_t>(size));
        wl_buffer*   buffer = wl_shm_pool_create_buffer(
            pool, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), stride, WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        ::close(fd);

        wl_buffer_add_listener(buffer, &s_bufferListener, nullptr);

        wl_surface_attach(m_surface, buffer, 0, 0);
        wl_surface_damage_buffer(m_surface, 0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height));
        wl_surface_commit(m_surface);
    }

    // Entrada (touch/teclado/ratón) --------------------------------------------------------------
    // No-op: IWindow no expone todavía una API de entrada. Estos callbacks solo satisfacen el
    // protocolo Wayland (los listeners deben estar completos); se rellenarán cuando el motor
    // tenga un sistema de Input al que reenviar estos eventos.

    void WaylandWindow::onTouchDown(void*, wl_touch*, uint32_t, uint32_t, wl_surface*, int32_t, wl_fixed_t, wl_fixed_t) {}
    void WaylandWindow::onTouchUp(void*, wl_touch*, uint32_t, uint32_t, int32_t) {}
    void WaylandWindow::onTouchMotion(void*, wl_touch*, uint32_t, int32_t, wl_fixed_t, wl_fixed_t) {}
    void WaylandWindow::onTouchFrame(void*, wl_touch*) {}
    void WaylandWindow::onTouchCancel(void*, wl_touch*) {}
    void WaylandWindow::onTouchShape(void*, wl_touch*, int32_t, wl_fixed_t, wl_fixed_t) {}
    void WaylandWindow::onTouchOrientation(void*, wl_touch*, int32_t, wl_fixed_t) {}

    void WaylandWindow::onKeyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t fd, uint32_t) { ::close(fd); }
    void WaylandWindow::onKeyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
    void WaylandWindow::onKeyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*) {}
    void WaylandWindow::onKeyboardKey(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t) {}
    void WaylandWindow::onKeyboardModifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
    void WaylandWindow::onKeyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

    void WaylandWindow::onPointerEnter(void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {}
    void WaylandWindow::onPointerLeave(void*, wl_pointer*, uint32_t, wl_surface*) {}
    void WaylandWindow::onPointerMotion(void*, wl_pointer*, uint32_t, wl_fixed_t, wl_fixed_t) {}
    void WaylandWindow::onPointerButton(void*, wl_pointer*, uint32_t, uint32_t, uint32_t, uint32_t) {}
    void WaylandWindow::onPointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}
    void WaylandWindow::onPointerFrame(void*, wl_pointer*) {}
    void WaylandWindow::onPointerAxisSource(void*, wl_pointer*, uint32_t) {}
    void WaylandWindow::onPointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
    void WaylandWindow::onPointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}

    // Constructor / Destructor -------------------------------------------------------------------

    WaylandWindow::WaylandWindow(const IWindow::Desc& desc) : m_width(desc.width), m_height(desc.height) {
        m_display = wl_display_connect(nullptr);
        if (!m_display) {
            LOG_ERROR(k_category, "wl_display_connect failed — WAYLAND_DISPLAY not set?");
            return;
        }

        m_registry = wl_display_get_registry(m_display);
        wl_registry_add_listener(m_registry, &s_registryListener, this);
        wl_display_roundtrip(m_display);   // populates m_compositor, m_wmBase, m_seat

        if (!m_compositor || !m_wmBase || !m_shm) {
            LOG_ERROR(k_category, "Required Wayland globals not advertised (compositor/xdg_wm_base/shm).");
            return;
        }

        m_surface = wl_compositor_create_surface(m_compositor);
        m_xdgSurface = xdg_wm_base_get_xdg_surface(m_wmBase, m_surface);
        m_toplevel = xdg_surface_get_toplevel(m_xdgSurface);

        xdg_wm_base_add_listener(m_wmBase, &s_wmBaseListener, this);
        xdg_surface_add_listener(m_xdgSurface, &s_xdgSurfaceListener, this);
        xdg_toplevel_add_listener(m_toplevel, &s_toplevelListener, this);

        xdg_toplevel_set_title(m_toplevel, desc.title.c_str());
        if (!desc.resizable) xdg_toplevel_set_max_size(m_toplevel, static_cast<int32_t>(desc.width), static_cast<int32_t>(desc.height));

        wl_surface_commit(m_surface);
        wl_display_roundtrip(m_display);   // triggers configure → m_configured

        m_open = true;
        LOGF_INFO(k_category, "Wayland window '{}' created {}x{}.", desc.title, m_width, m_height);
    }

    WaylandWindow::~WaylandWindow() {
        if (m_touch)      { wl_touch_destroy(m_touch);           m_touch = nullptr; }
        if (m_pointer)    { wl_pointer_destroy(m_pointer);       m_pointer = nullptr; }
        if (m_keyboard)   { wl_keyboard_destroy(m_keyboard);     m_keyboard = nullptr; }
        if (m_seat)       { wl_seat_destroy(m_seat);             m_seat = nullptr; }
        if (m_toplevel)   { xdg_toplevel_destroy(m_toplevel);    m_toplevel = nullptr; }
        if (m_xdgSurface) { xdg_surface_destroy(m_xdgSurface);   m_xdgSurface = nullptr; }
        if (m_surface)    { wl_surface_destroy(m_surface);       m_surface = nullptr; }
        if (m_wmBase)     { xdg_wm_base_destroy(m_wmBase);       m_wmBase = nullptr; }
        if (m_shm)        { wl_shm_destroy(m_shm);               m_shm = nullptr; }
        if (m_compositor) { wl_compositor_destroy(m_compositor); m_compositor = nullptr; }
        if (m_registry)   { wl_registry_destroy(m_registry);     m_registry = nullptr; }
        if (m_display)    { wl_display_disconnect(m_display);    m_display = nullptr; }
    }

    // IWindow ------------------------------------------------------------------------------------

    bool WaylandWindow::poll_events() {
        if (!m_display) return false;

        // Lectura no bloqueante del socket Wayland: drena cualquier evento ya en cola, envía las
        // peticiones salientes pendientes, y solo lee del fd si hay datos disponibles ahora mismo.
        while (wl_display_prepare_read(m_display) != 0) {
            wl_display_dispatch_pending(m_display);
        }
        wl_display_flush(m_display);

        pollfd pfd{ wl_display_get_fd(m_display), POLLIN, 0 };
        if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
            wl_display_read_events(m_display);
            wl_display_dispatch_pending(m_display);
        } else {
            wl_display_cancel_read(m_display);
        }

        return m_open;
    }

    void WaylandWindow::close() { m_open = false; }

    bool     WaylandWindow::is_open()       const noexcept { return m_open; }
    uint32_t WaylandWindow::width()         const noexcept { return m_width; }
    uint32_t WaylandWindow::height()        const noexcept { return m_height; }
    void*    WaylandWindow::native_handle() const noexcept { return m_surface; }
} // namespace anxiety::platform
