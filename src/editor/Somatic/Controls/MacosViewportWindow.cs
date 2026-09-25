using System.Runtime.InteropServices;

namespace Somatic.Controls {
    // MacosViewportWindow ----------------------------------------------------------------------------
    // NSView hija sobre la que el motor dibuja en macOS. Se añade como subvista real del NSView que
    // Avalonia entrega como padre al hospedar un control nativo — a diferencia de Win32/X11, en AppKit
    // no existe un equivalente de "ventana hija ligera"; la única forma de embeber contenido dentro del
    // árbol de vistas de Avalonia es una NSView de verdad. El motor dibuja directamente sobre ella
    // (GLDevice / VulkanSwapchain / MetalSwapchain tratan el handle como NSView*, no como NSWindow* —
    // ver el comentario en GLDevice.cpp).
    //
    // Sin binding gestionado de AppKit en el proyecto: se invoca el runtime de Objective-C directamente
    // (objc_msgSend) igual que haría cualquier binding nativo. No se registra una subclase propia de
    // NSView: una NSView simple no repinta nada por sí sola (a diferencia del "Static" de Win32 o una
    // ventana X11 con background pixmap), así que no hace falta el equivalente al DefWindowProc /
    // CWBackPixmap de las otras dos plataformas.
    // ---------------------------------------------------------------------------------------------
    internal static class MacosViewportWindow {
        private const string ObjCLib = "/usr/lib/libobjc.A.dylib";

        [StructLayout(LayoutKind.Sequential)]
        private struct NSRect {
            public double X, Y, Width, Height;
        }

        [DllImport(ObjCLib)]
        private static extern nint objc_getClass(string name);

        [DllImport(ObjCLib)]
        private static extern nint sel_registerName(string name);

        // objc_msgSend no tiene una única firma: cada combinación de tipos de argumento/retorno necesita
        // su propia declaración P/Invoke sobre el mismo símbolo nativo.
        [DllImport(ObjCLib, EntryPoint = "objc_msgSend")]
        private static extern nint SendMsg_ReturnsPtr(nint receiver, nint selector);

        [DllImport(ObjCLib, EntryPoint = "objc_msgSend")]
        private static extern nint SendMsg_ReturnsPtr_NSRect(nint receiver, nint selector, NSRect frame);

        [DllImport(ObjCLib, EntryPoint = "objc_msgSend")]
        private static extern void SendMsg_Void_Ptr(nint receiver, nint selector, nint arg);

        [DllImport(ObjCLib, EntryPoint = "objc_msgSend")]
        private static extern void SendMsg_Void(nint receiver, nint selector);

        private static readonly nint s_nsViewClass          = objc_getClass("NSView");
        private static readonly nint s_selAlloc             = sel_registerName("alloc");
        private static readonly nint s_selInitWithFrame     = sel_registerName("initWithFrame:");
        private static readonly nint s_selAddSubview        = sel_registerName("addSubview:");
        private static readonly nint s_selRelease           = sel_registerName("release");
        private static readonly nint s_selRemoveFromSuperview = sel_registerName("removeFromSuperview");

        /// <summary>Crea la NSView hija dentro de <paramref name="parentView"/>. Avalonia la coloca y dimensiona después.</summary>
        public static nint Create(nint parentView) {
            nint allocated = SendMsg_ReturnsPtr(s_nsViewClass, s_selAlloc);
            nint view      = SendMsg_ReturnsPtr_NSRect(allocated, s_selInitWithFrame, new NSRect { X = 0, Y = 0, Width = 1, Height = 1 });
            if (view == 0) throw new InvalidOperationException("[[NSView alloc] initWithFrame:] falló: no se pudo crear la vista nativa del viewport.");

            // addSubview: retiene la vista; se suelta la propiedad de alloc/init para que el padre sea
            // el único dueño (mismo patrón de gestión de memoria que cualquier código Cocoa manual).
            SendMsg_Void_Ptr(parentView, s_selAddSubview, view);
            SendMsg_Void(view, s_selRelease);

            return view;
        }

        public static void Destroy(nint view) {
            if (view == 0) return;

            SendMsg_Void(view, s_selRemoveFromSuperview);
        }
    }
}
