using Somatic.Controls.Inspector;
using Somatic.Core.Scene.Components;

namespace Somatic.Views.Inspector {
    public partial class LightComponentEditor : ComponentEditor {
        public static IReadOnlyList<LightType> LightTypes { get; } = Enum.GetValues<LightType>();

        public LightComponentEditor() {
            InitializeComponent();
        }
    }
}
