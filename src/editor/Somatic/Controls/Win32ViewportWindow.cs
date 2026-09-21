using System.ComponentModel;
using System.Runtime.InteropServices;

namespace Somatic.Controls {
    // Win32ViewportWindow --------------------------------------------------------------------------
    // Ventana hija nativa (HWND) sobre la que el motor crea su swapchain. Se registra una clase propia
    // SIN pincel de fondo y con DefWindowProc: así Windows no repinta (GDI) sobre lo que dibuja el motor
    // al mover o redimensionar la ventana, cosa que sí hace una clase estándar como "Static".
    // ---------------------------------------------------------------------------------------------
    internal static class Win32ViewportWindow {
        private const string ClassName = "AnxietyViewportWindow";

        private const uint WS_CHILD        = 0x40000000;
        private const uint WS_VISIBLE      = 0x10000000;
        private const uint WS_CLIPSIBLINGS = 0x04000000;
        private const uint WS_CLIPCHILDREN = 0x02000000;
        private const int  IDC_ARROW       = 32512;

        private static readonly object Gate = new();
        private static bool s_classRegistered;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct WNDCLASSEXW {
            public uint   cbSize;
            public uint   style;
            public nint   lpfnWndProc;
            public int    cbClsExtra;
            public int    cbWndExtra;
            public nint   hInstance;
            public nint   hIcon;
            public nint   hCursor;
            public nint   hbrBackground;
            public string? lpszMenuName;
            public string? lpszClassName;
            public nint   hIconSm;
        }

        [DllImport("user32", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern ushort RegisterClassExW(in WNDCLASSEXW wc);

        [DllImport("user32", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern nint CreateWindowExW(uint exStyle, string className, string? windowName, uint style, int x, int y, int width, int height, nint parent, nint menu, nint instance, nint param);

        [DllImport("user32", SetLastError = true)]
        private static extern bool DestroyWindow(nint hwnd);

        [DllImport("user32", CharSet = CharSet.Unicode)]
        private static extern nint LoadCursorW(nint instance, nint cursorName);

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern nint GetModuleHandleW(string? moduleName);

        private static void EnsureClassRegistered() {
            lock (Gate) {
                if (s_classRegistered) return;

                WNDCLASSEXW wc = new() {
                    cbSize        = (uint)Marshal.SizeOf<WNDCLASSEXW>(),
                    lpfnWndProc   = NativeLibrary.GetExport(NativeLibrary.Load("user32"), "DefWindowProcW"),
                    hInstance     = GetModuleHandleW(null),
                    hCursor       = LoadCursorW(0, IDC_ARROW),
                    hbrBackground = 0,
                    lpszClassName = ClassName
                };

                if (RegisterClassExW(wc) == 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "No se pudo registrar la clase de ventana del viewport.");
                s_classRegistered = true;
            }
        }

        /// <summary>Crea la ventana hija dentro de <paramref name="parent"/>. Avalonia la coloca y dimensiona después.</summary>
        public static nint Create(nint parent) {
            EnsureClassRegistered();

            nint hwnd = CreateWindowExW(0, ClassName, null, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, 0, GetModuleHandleW(null), 0);
            if (hwnd == 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "No se pudo crear la ventana nativa del viewport.");

            return hwnd;
        }

        public static void Destroy(nint hwnd) {
            if (hwnd != 0) DestroyWindow(hwnd);
        }
    }
}
