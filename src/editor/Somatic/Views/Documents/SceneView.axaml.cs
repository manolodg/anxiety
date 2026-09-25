using Avalonia.Controls;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Controls;
using Somatic.Core.Viewport;

namespace Somatic.Views.Documents {
    public partial class SceneView : UserControl {
        public SceneView() {
            InitializeComponent();

            if (Design.IsDesignMode) return;

            IViewportSurfaceFactory? factory = App.Services?.GetService<IViewportSurfaceFactory>();
            if (factory is null) return;

            IViewportSurface surface = factory.Create(new ViewportId("scene"));
            if (surface.State == ViewportState.Unavailable) {
                PlaceholderText.Text = "Motor no disponible";
                surface.Dispose();
                return;
            }

            ViewportHostContainer.Child = new EngineViewportHost { Surface = surface };
        }
    }
}
