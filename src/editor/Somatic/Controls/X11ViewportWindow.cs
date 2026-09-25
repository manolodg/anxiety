using System.Runtime.InteropServices;

namespace Somatic.Controls {
    // X11ViewportWindow ------------------------------------------------------------------------------
    // Ventana hija nativa (Window XID) sobre la que el motor crea su swapchain en Linux/X11. Se crea
    // con CWBackPixmap = None: así el servidor X no repinta un fondo sobre lo que dibuja el motor al
    // exponer o redimensionar la ventana — el equivalente X11 del "sin pincel de fondo" de
    // Win32ViewportWindow.
    //
    // No se selecciona ninguna máscara de eventos: al no seleccionarlos aquí, los eventos de entrada
    // se propagan automáticamente al ancestro (la ventana de Avalonia) que sí los selecciona.
    //
    // Usa su propia conexión Xlib (XOpenDisplay), distinta de la que gestiona Avalonia.X11 internamente
    // y distinta también de la que abre el motor (VulkanSwapchain/GLDevice) para consultar la geometría
    // de la ventana. Es válido por protocolo — X soporta múltiples clientes — pero implica que hay que
    // redimensionar la ventana desde AQUÍ, de forma síncrona (XSync), antes de llamar a Attach()/Resize()
    // en el motor: si se dejara que Avalonia.X11 la redimensionara por su propia conexión, la petición
    // podría no haber llegado aún al servidor X cuando el motor consulta el tamaño por la suya, y crearía
    // el swapchain con la geometría vieja (visto en la práctica: swapchain 1x1 con la ventana ya a
    // 856x600 — EngineViewportHost.SyncSurface() llama a Resize() antes de Attach()/Resize() del motor).
    // ---------------------------------------------------------------------------------------------
    internal static class X11ViewportWindow {
        private const int  CopyFromParent = 0;
        private const int  InputOutput    = 1;
        private const nint CWBackPixmap   = 1 << 0;

        [StructLayout(LayoutKind.Sequential)]
        private struct XSetWindowAttributes {
            public nint background_pixmap;
            public nint background_pixel;
            public nint border_pixmap;
            public nint border_pixel;
            public int  bit_gravity;
            public int  win_gravity;
            public int  backing_store;
            public nint backing_planes;
            public nint backing_pixel;
            public int  save_under;
            public nint event_mask;
            public nint do_not_propagate_mask;
            public int  override_redirect;
            public nint colormap;
            public nint cursor;
        }

        [DllImport("libX11.so.6")]
        private static extern nint XOpenDisplay(nint displayName);

        [DllImport("libX11.so.6")]
        private static extern nint XCreateWindow(nint display, nint parent, int x, int y, uint width, uint height, uint borderWidth, int depth, uint windowClass, nint visual, nint valueMask, ref XSetWindowAttributes attributes);

        [DllImport("libX11.so.6")]
        private static extern int XMapWindow(nint display, nint window);

        [DllImport("libX11.so.6")]
        private static extern int XDestroyWindow(nint display, nint window);

        [DllImport("libX11.so.6")]
        private static extern int XFlush(nint display);

        [DllImport("libX11.so.6")]
        private static extern int XResizeWindow(nint display, nint window, uint width, uint height);

        [DllImport("libX11.so.6")]
        private static extern int XSync(nint display, int discard);

        private static readonly object Gate = new();
        private static nint s_display;

        private static nint EnsureDisplay() {
            lock (Gate) {
                if (s_display != 0) return s_display;

                s_display = XOpenDisplay(0);
                if (s_display == 0) throw new InvalidOperationException("XOpenDisplay() falló: no se pudo conectar con el servidor X (¿$DISPLAY definido?).");

                return s_display;
            }
        }

        /// <summary>Crea la ventana hija dentro de <paramref name="parent"/>. Avalonia la coloca y dimensiona después.</summary>
        public static nint Create(nint parent) {
            nint display = EnsureDisplay();

            XSetWindowAttributes attrs = default;
            attrs.background_pixmap = 0; // None: el motor dibuja directamente, sin que X limpie el fondo antes.

            nint window = XCreateWindow(display, parent, 0, 0, 1, 1, 0, CopyFromParent, InputOutput, 0, CWBackPixmap, ref attrs);
            if (window == 0) throw new InvalidOperationException("XCreateWindow() falló: no se pudo crear la ventana nativa del viewport.");

            XMapWindow(display, window);
            XFlush(display);

            return window;
        }

        /// <summary>
        /// Redimensiona la ventana y espera (XSync) a que el servidor X confirme el cambio, de modo que
        /// una consulta de geometría inmediatamente posterior desde OTRA conexión Xlib (la del motor) ya
        /// vea el tamaño nuevo. Hay que llamarla antes de Attach()/Resize() en el motor.
        /// </summary>
        public static void Resize(nint window, int width, int height) {
            if (window == 0 || s_display == 0 || width <= 0 || height <= 0) return;

            XResizeWindow(s_display, window, (uint)width, (uint)height);
            XSync(s_display, 0);
        }

        public static void Destroy(nint window) {
            if (window == 0 || s_display == 0) return;

            XDestroyWindow(s_display, window);
            XFlush(s_display);
        }
    }
}
