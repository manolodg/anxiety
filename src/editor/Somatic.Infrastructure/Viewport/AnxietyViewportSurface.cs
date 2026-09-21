using Somatic.Core.Viewport;

namespace Somatic.Infrastructure.Viewport {
    // AnxietyViewportSurface -----------------------------------------------------------------------
    // Superficie respaldada por anxiety_bridge. Toda la lógica y el estado compartido (un único
    // swapchain a la vez) viven en AnxietyEngineHost; esta clase solo valida argumentos y delega.
    // ---------------------------------------------------------------------------------------------
    internal sealed class AnxietyViewportSurface(AnxietyEngineHost host, ViewportId id) : IViewportSurface {
        public ViewportId    Id    { get; } = id;
        public ViewportState State { get; internal set; } = ViewportState.Available;

        public void Resize(int width, int height) {
            if (width < 0)  throw new ArgumentOutOfRangeException(nameof(width), width, "El ancho del viewport no puede ser negativo.");
            if (height < 0) throw new ArgumentOutOfRangeException(nameof(height), height, "El alto del viewport no puede ser negativo.");

            host.Resize(this, width, height);
        }

        public bool Attach(nint nativeWindowHandle, int width, int height) {
            if (width < 0)  throw new ArgumentOutOfRangeException(nameof(width), width, "El ancho del viewport no puede ser negativo.");
            if (height < 0) throw new ArgumentOutOfRangeException(nameof(height), height, "El alto del viewport no puede ser negativo.");

            return host.Attach(this, nativeWindowHandle, width, height);
        }

        public void Detach() => host.Detach(this);

        public void Dispose() => host.Release(this);
    }
}
