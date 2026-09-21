namespace Somatic.Core.Viewport {
    /// <summary>
    /// Sin operación <see cref="IViewportSurface"/> para una ventana gráfica sin backend asociado.
    /// Permance en estado <see cref="ViewportState.Unavailable"/> durante todo su ciclo de vida; todavía no hay
    /// nada que pueda hacer que pase a estado <see cref="ViewportState.Available"/>.
    /// </summary>
    public sealed class NullViewportSurface(ViewportId id) : IViewportSurface {
        public ViewportId    Id    { get; } = id;
        public ViewportState State { get; } = ViewportState.Unavailable;

        public void Resize(int width, int height) {
            if (width < 0)  throw new ArgumentOutOfRangeException(nameof(width), width, "El ancho del viewport no puede ser negativo.");
            if (height < 0) throw new ArgumentOutOfRangeException(nameof(height), height, "El alto del viewport no puede ser negativo.");
        }

        public void Attach() { }
        public void Detach() { }

        public void Dispose() { }
    }
}
