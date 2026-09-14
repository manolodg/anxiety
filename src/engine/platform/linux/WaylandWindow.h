#pragma once

// Cabeceras de cliente Wayland (libwayland-client-dev)
#include <wayland-client.h>

// Generado por wayland-scanner desde xdg-shell.xml (hecho en CMakeLists.txt).
// Localizado en ${CMAKE_CURRENT_BINARY_DIR}/wayland-gen/
#include "xdg-shell-client-protocol.h"

#include "IWindow.h"

#include <cstdint>

namespace anxiety::platform {
    // WaylandWindow ------------------------------------------------------------------------------
    // IWindow backend con una superficie Wayland nativa + XDG shell toplevel.
    //
    // Requiere un compositor Wayland en ejecución (WAYLAND_DISPLAY en env). La cabecera de
    // protocolo xdg-shell es generada en la configuración de CMake mediante wayland-scanner.
    //
    // Nota: se vinculan los listeners de seat / teclado / ratón / touch porque el protocolo Wayland
    // exige responder a ellos (p. ej. wl_registry.global, xdg_wm_base.ping), pero IWindow todavía no
    // expone una API de entrada — los callbacks de entrada son no-op hasta que el motor tenga un
    // sistema de Input.
    // --------------------------------------------------------------------------------------------
    class WaylandWindow final : public IWindow {
    public:
        explicit WaylandWindow(const IWindow::Desc& desc);
        ~WaylandWindow() override;

        [[nodiscard]] bool     poll_events()                  override;
        void                   close()                        override;
        [[nodiscard]] bool     is_open()       const noexcept override;
        [[nodiscard]] uint32_t width()         const noexcept override;
        [[nodiscard]] uint32_t height()        const noexcept override;
        [[nodiscard]] void*    native_handle() const noexcept override;

    private:
        // Callbacks de protocolo -------------------------------------------------------------------
        static void onRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
        static void onRegistryGlobalRemove(void* data, wl_registry* registry, uint32_t name);

        static void onWmBasePing(void* data, xdg_wm_base* wmBase, uint32_t serial);

        static void onXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial);

        static void onToplevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
        static void onToplevelClose(void* data, xdg_toplevel* toplevel);
        static void onToplevelConfigureBounds(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height);
        static void onToplevelWmCapabilities(void* data, xdg_toplevel* toplevel, wl_array* capabilities);

        static void onSeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
        static void onSeatName(void* data, wl_seat* seat, const char* name);

        static void onBufferRelease(void* data, wl_buffer* buffer);

        static void onTouchDown(void* data, wl_touch* touch, uint32_t serial, uint32_t time, wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y);
        static void onTouchUp(void* data, wl_touch* touch, uint32_t serial, uint32_t time, int32_t id);
        static void onTouchMotion(void* data, wl_touch* touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y);
        static void onTouchFrame(void* data, wl_touch* touch);
        static void onTouchCancel(void* data, wl_touch* touch);
        static void onTouchShape(void* data, wl_touch* touch, int32_t id, wl_fixed_t major, wl_fixed_t minor);
        static void onTouchOrientation(void* data, wl_touch* touch, int32_t id, wl_fixed_t orientation);

        static void onKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size);
        static void onKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
        static void onKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
        static void onKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
        static void onKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
        static void onKeyboardRepeatInfo(void* data, wl_keyboard* keyboard, int32_t rate, int32_t delay);

        static void onPointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t surfaceX, wl_fixed_t surfaceY);
        static void onPointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
        static void onPointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t surfaceX, wl_fixed_t surfaceY);
        static void onPointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
        static void onPointerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
        static void onPointerFrame(void* data, wl_pointer* pointer);
        static void onPointerAxisSource(void* data, wl_pointer* pointer, uint32_t axisSource);
        static void onPointerAxisStop(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis);
        static void onPointerAxisDiscrete(void* data, wl_pointer* pointer, uint32_t axis, int32_t discrete);

        static const wl_registry_listener  s_registryListener;
        static const xdg_wm_base_listener  s_wmBaseListener;
        static const xdg_surface_listener  s_xdgSurfaceListener;
        static const xdg_toplevel_listener s_toplevelListener;
        static const wl_seat_listener      s_seatListener;
        static const wl_touch_listener     s_touchListener;
        static const wl_keyboard_listener  s_keyboardListener;
        static const wl_pointer_listener   s_pointerListener;
        static const wl_buffer_listener    s_bufferListener;

        // Crea un buffer wl_shm relleno de un color sólido para el tamaño actual y lo adjunta a la
        // superficie. Placeholder hasta que exista un backend de render real (EGL/Vulkan).
        void attachFrame();

        // Objetos Wayland  -----------------------------------------------------------------------
        wl_display*    m_display    { nullptr };
        wl_registry*   m_registry   { nullptr };
        wl_compositor* m_compositor { nullptr };
        wl_surface*    m_surface    { nullptr };
        xdg_wm_base*   m_wmBase     { nullptr };
        xdg_surface*   m_xdgSurface { nullptr };
        xdg_toplevel*  m_toplevel   { nullptr };
        wl_seat*       m_seat       { nullptr };
        wl_pointer*    m_pointer    { nullptr };
        wl_keyboard*   m_keyboard   { nullptr };
        wl_touch*      m_touch      { nullptr };
        wl_shm*        m_shm        { nullptr };

        // State ----------------------------------------------------------------------------------
        uint32_t m_width      { 0 };
        uint32_t m_height     { 0 };
        bool     m_open       { false };
        bool     m_configured { false };
    };
} // namespace anxiety::platform
