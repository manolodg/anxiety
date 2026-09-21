namespace Somatic.Core.Viewport {
    public interface IViewportSurface : IDisposable {
        ViewportId    Id    { get; }
        ViewportState State { get; }

        void Resize(int width, int height);
        void Attach();
        void Detach();
    }
}
