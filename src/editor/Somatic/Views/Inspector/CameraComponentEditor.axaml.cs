using Somatic.Controls.Inspector;
using Somatic.Core.Scene.Components;

namespace Somatic.Views.Inspector {
    public partial class CameraComponentEditor : ComponentEditor {
        public static IReadOnlyList<CameraProjection> Projections { get; } = Enum.GetValues<CameraProjection>();

        public CameraComponentEditor() {
            InitializeComponent();
        }
    }
}
