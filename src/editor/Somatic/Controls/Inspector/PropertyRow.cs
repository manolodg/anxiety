using Avalonia;
using Avalonia.Controls;

namespace Somatic.Controls.Inspector {
    /// <summary>Fila "etiqueta | editor" del inspector. La plantilla está en InspectorTheme.axaml.</summary>
    public class PropertyRow : ContentControl {
        public static readonly StyledProperty<string?> LabelProperty =
            AvaloniaProperty.Register<PropertyRow, string?>(nameof(Label));

        public string? Label {
            get => GetValue(LabelProperty);
            set => SetValue(LabelProperty, value);
        }
    }
}
