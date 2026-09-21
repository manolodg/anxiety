using System.Runtime.InteropServices;
using Microsoft.Extensions.Logging;
using Somatic.Core.Viewport;

namespace Somatic.Infrastructure.Viewport {
    // AnxietyEngineHost ----------------------------------------------------------------------------
    // Posee el motor nativo (anxiety_bridge) del proceso: lo arranca la primera vez que se crea una
    // superficie y lo detiene al eliminarse el contenedor de servicios. Si la DLL no está o el motor no
    // arranca, entrega NullViewportSurface y la interfaz sigue funcionando con su placeholder.
    //
    // El motor solo dibuja en una ventana a la vez: si otra superficie ya está adjunta, Attach falla.
    // Toda llamada nativa se serializa con _gate; las nativas son rápidas o no bloqueantes salvo attach
    // y detach, que esperan como mucho al fotograma en curso.
    // ---------------------------------------------------------------------------------------------
    internal sealed class AnxietyEngineHost(ILogger<AnxietyEngineHost> logger) : IViewportSurfaceFactory, IDisposable {
        private readonly object _gate = new();
        private nint _engine;
        private bool _startAttempted;
        private bool _disposed;
        private AnxietyViewportSurface? _attached;

        public string? BackendName { get; private set; }

        public IViewportSurface Create(ViewportId id) {
            lock (_gate) {
                ObjectDisposedException.ThrowIf(_disposed, this);
                EnsureStarted();
                return _engine != 0 ? new AnxietyViewportSurface(this, id) : new NullViewportSurface(id);
            }
        }

        // Arranque perezoso, un único intento: si falla no se reintenta en cada superficie.
        private void EnsureStarted() {
            if (_startAttempted) return;
            _startAttempted = true;

            try {
                _engine = AnxietyBridgeInterop.anxiety_bridge_create();
            } catch (Exception e) when (e is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException) {
                logger.LogWarning(e, "No se pudo cargar anxiety_bridge; los viewports quedarán sin motor.");
                return;
            }

            if (_engine == 0) {
                logger.LogError("anxiety_bridge_create falló: el motor no arrancó.");
                return;
            }

            BackendName = Marshal.PtrToStringUTF8(AnxietyBridgeInterop.anxiety_bridge_backend_name(_engine));
            logger.LogInformation("Motor de Anxiety en marcha, backend: {Backend}", BackendName);
        }

        internal bool Attach(AnxietyViewportSurface surface, nint nativeWindow, int width, int height) {
            lock (_gate) {
                if (_disposed || _engine == 0 || nativeWindow == 0 || width <= 0 || height <= 0) return false;

                if (_attached is not null && !ReferenceEquals(_attached, surface)) {
                    logger.LogWarning("El viewport {Viewport} no puede adjuntarse: el motor ya dibuja en {Other}.", surface.Id, _attached.Id);
                    return false;
                }

                if (AnxietyBridgeInterop.anxiety_bridge_attach_window(_engine, nativeWindow, (uint)width, (uint)height) == 0) {
                    logger.LogError("El motor no pudo crear el swapchain para el viewport {Viewport}.", surface.Id);
                    return false;
                }

                _attached     = surface;
                surface.State = ViewportState.Attached;
                return true;
            }
        }

        internal void Resize(AnxietyViewportSurface surface, int width, int height) {
            lock (_gate) {
                if (_disposed || _engine == 0 || !ReferenceEquals(_attached, surface) || width == 0 || height == 0) return;

                AnxietyBridgeInterop.anxiety_bridge_resize(_engine, (uint)width, (uint)height);
            }
        }

        internal void Detach(AnxietyViewportSurface surface) {
            lock (_gate) {
                if (!ReferenceEquals(_attached, surface)) return;

                DetachLocked();
            }
        }

        internal void Release(AnxietyViewportSurface surface) => Detach(surface);

        private void DetachLocked() {
            if (_attached is null) return;

            if (_engine != 0) AnxietyBridgeInterop.anxiety_bridge_detach_window(_engine);
            _attached.State = ViewportState.Available;
            _attached       = null;
        }

        public void Dispose() {
            lock (_gate) {
                if (_disposed) return;
                _disposed = true;

                // Primero se suelta la ventana y solo después se detiene el motor.
                DetachLocked();

                if (_engine != 0) {
                    AnxietyBridgeInterop.anxiety_bridge_destroy(_engine);
                    _engine = 0;
                }
            }
        }
    }
}
