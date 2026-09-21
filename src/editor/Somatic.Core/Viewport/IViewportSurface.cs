namespace Somatic.Core.Viewport {
    /// <summary>
    /// Superficie de dibujado del motor dentro de una ventana gráfica del editor. La ventana nativa
    /// (HWND / Window X11 / NSView) es propiedad del llamador: la superficie solo le dice al motor que
    /// dibuje en ella.
    /// </summary>
    public interface IViewportSurface : IDisposable {
        ViewportId    Id    { get; }
        ViewportState State { get; }

        /// <summary>Nuevo tamaño en píxeles físicos. No bloquea; tamaño 0 (minimizada) se ignora.</summary>
        void Resize(int width, int height);

        /// <summary>
        /// Adjunta la ventana nativa. Devuelve <c>false</c> si el motor no está disponible o el backend
        /// gráfico no pudo crear el swapchain; en ese caso el estado no cambia.
        /// </summary>
        bool Attach(nint nativeWindowHandle, int width, int height);

        /// <summary>
        /// Suelta la ventana. Es síncrona: al volver, el motor ya no la usa. Hay que llamarla ANTES de
        /// destruir la ventana nativa.
        /// </summary>
        void Detach();
    }
}
