using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
using Somatic.Core.Viewport;

namespace Somatic.Controls {
    // EngineViewportHost ---------------------------------------------------------------------------
    // Aloja una ventana nativa hija (HWND) sobre la que el motor dibuja a través de una IViewportSurface.
    //
    // Ciclo de vida (lo gobierna NativeControlHost al entrar y salir del árbol visual):
    //   crear HWND  ->  primer Arrange con tamaño válido: surface.Attach(hwnd)  ->  cada cambio de tamaño
    //   (o de escala DPI): surface.Resize()  ->  al salir del árbol: surface.Detach() y DestroyWindow.
    //
    // Regla crítica: la superficie se suelta ANTES de destruir la ventana, si no el swapchain del motor
    // quedaría apuntando a un HWND muerto.
    //
    // Solo Windows por ahora; en otros SO no se crea ventana nativa (ver SceneView, que no lo usa).
    // ---------------------------------------------------------------------------------------------
    internal sealed class EngineViewportHost : NativeControlHost {
        private IViewportSurface? _surface;
        private nint              _hwnd;
        private TopLevel?         _topLevel;
        private Size              _lastSize;
        private bool              _attachFailed;
        private int               _lastWidth;
        private int               _lastHeight;

        /// <summary>Superficie del motor a la que se entrega la ventana. Hay que asignarla antes de entrar al árbol visual.</summary>
        public IViewportSurface? Surface {
            get => _surface;
            set {
                if (_hwnd != 0) throw new InvalidOperationException("No se puede cambiar la superficie con la ventana nativa ya creada.");
                _surface = value;
            }
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e) {
            base.OnAttachedToVisualTree(e);

            _topLevel = TopLevel.GetTopLevel(this);
            if (_topLevel is not null) _topLevel.ScalingChanged += OnScalingChanged;
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e) {
            if (_topLevel is not null) _topLevel.ScalingChanged -= OnScalingChanged;
            _topLevel = null;

            base.OnDetachedFromVisualTree(e);
        }

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent) {
            if (!OperatingSystem.IsWindows() || _surface is null) return base.CreateNativeControlCore(parent);

            _hwnd         = Win32ViewportWindow.Create(parent.Handle);
            _attachFailed = false;
            _lastWidth    = 0;
            _lastHeight   = 0;

            return new PlatformHandle(_hwnd, "HWND");
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control) {
            if (_hwnd == 0 || control.Handle != _hwnd) {
                base.DestroyNativeControlCore(control);
                return;
            }

            // Primero se suelta la superficie (síncrono) y solo después se destruye la ventana.
            _surface?.Detach();
            Win32ViewportWindow.Destroy(_hwnd);
            _hwnd = 0;
        }

        protected override Size ArrangeOverride(Size finalSize) {
            // El base coloca y dimensiona la ventana nativa; el motor se sincroniza DESPUÉS, de modo que
            // cuando aplique el resize la ventana ya tiene su tamaño final.
            Size result = base.ArrangeOverride(finalSize);

            _lastSize = finalSize;
            SyncSurface();

            return result;
        }

        private void OnScalingChanged(object? sender, EventArgs e) => SyncSurface();

        // Convierte el tamaño (unidades independientes de DPI) a píxeles físicos y se lo pasa al motor.
        private void SyncSurface() {
            if (_surface is null || _hwnd == 0) return;

            double scale  = _topLevel?.RenderScaling ?? 1.0;
            int    width  = (int)Math.Round(_lastSize.Width  * scale);
            int    height = (int)Math.Round(_lastSize.Height * scale);
            if (width <= 0 || height <= 0) return;

            if (_surface.State == ViewportState.Attached) {
                if (width == _lastWidth && height == _lastHeight) return;

                _lastWidth  = width;
                _lastHeight = height;
                _surface.Resize(width, height);
                return;
            }

            // Un solo intento por ventana nativa: si el motor rechaza la ventana no se reintenta en cada layout.
            if (_attachFailed) return;

            if (_surface.Attach(_hwnd, width, height)) {
                _lastWidth  = width;
                _lastHeight = height;
            } else {
                _attachFailed = true;
            }
        }
    }
}
