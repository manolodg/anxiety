using System.Runtime.InteropServices;

namespace Somatic.Infrastructure.Viewport {
    // AnxietyBridgeInterop -------------------------------------------------------------------------
    // P/Invoke generado en compilación sobre la ABI en C de src/bridge/AnxietyBridge.h.
    // ------------------------------------------------------------------------------------------
    internal static partial class AnxietyBridgeInterop {
        private const string LibraryName = "anxiety_bridge";

        [LibraryImport(LibraryName)] public static partial nint anxiety_bridge_create();
        [LibraryImport(LibraryName)] public static partial void anxiety_bridge_destroy(nint handle);

        [LibraryImport(LibraryName)] public static partial int  anxiety_bridge_attach_window(nint handle, nint nativeWindow, uint width, uint height);
        [LibraryImport(LibraryName)] public static partial void anxiety_bridge_resize(nint handle, uint width, uint height);
        [LibraryImport(LibraryName)] public static partial void anxiety_bridge_detach_window(nint handle);

        // Devuelve un char* (UTF-8) propiedad del handle; NO hay que liberarlo, por eso se recibe como nint
        // y no como string (el marshaller de string intentaría liberar la memoria).
        [LibraryImport(LibraryName)] public static partial nint anxiety_bridge_backend_name(nint handle);
    }
}
