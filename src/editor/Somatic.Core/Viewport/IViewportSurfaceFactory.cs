namespace Somatic.Core.Viewport {
    /// <summary>
    /// Crea las <see cref="IViewportSurface"/> del editor. Si el motor nativo no está disponible (DLL
    /// ausente o fallo al arrancar) devuelve superficies <see cref="NullViewportSurface"/>, de modo que la
    /// interfaz siga funcionando mostrando un placeholder.
    /// </summary>
    public interface IViewportSurfaceFactory {
        /// <summary>Nombre del backend gráfico en uso (p. ej. "DirectX 12"), o <c>null</c> si el motor no está en marcha.</summary>
        string? BackendName { get; }

        IViewportSurface Create(ViewportId id);
    }
}
