using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Markup.Xaml.MarkupExtensions;
using Avalonia.Media;
using Somatic.Core.Scene;

namespace Somatic.Controls {
    /// <summary>Muestra de color más su valor hexadecimal editable (<c>#RRGGBB</c> o <c>#RRGGBBAA</c>).</summary>
    public class ColorField : UserControl {
        public static readonly StyledProperty<ColorRgba> ValueProperty =
            AvaloniaProperty.Register<ColorField, ColorRgba>(nameof(Value), ColorRgba.White, defaultBindingMode: BindingMode.TwoWay);

        private readonly Border  _swatch = new() { Width = 22, Margin = new Thickness(0, 0, 4, 0), BorderThickness = new Thickness(1) };
        private readonly TextBox _hex    = new();

        public ColorRgba Value {
            get => GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        public ColorField() {
            _swatch[!Border.BorderBrushProperty] = new DynamicResourceExtension("EditorBorderBrush");
            _hex.Classes.Add("InspectorField");
            _hex.KeyDown += (_, e) => {
                if (e.Key == Key.Enter) { Commit(); e.Handled = true; }
                else if (e.Key == Key.Escape) { Refresh(); e.Handled = true; }
            };
            _hex.LostFocus += (_, _) => Commit();

            DockPanel panel = new DockPanel();
            DockPanel.SetDock(_swatch, Avalonia.Controls.Dock.Left);
            panel.Children.Add(_swatch);
            panel.Children.Add(_hex);
            Content = panel;

            Refresh();
        }

        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);
            if (change.Property == ValueProperty) Refresh();
        }

        private void Commit() {
            if (ColorRgba.TryParseHex(_hex.Text, out ColorRgba color)) Value = color;
            Refresh();
        }

        private void Refresh() {
            ColorRgba value = Value;
            _swatch.Background = new SolidColorBrush(Color.FromArgb(ToByte(value.A), ToByte(value.R), ToByte(value.G), ToByte(value.B)));
            _hex.Text = value.ToHex();
        }

        private static byte ToByte(float channel) => (byte)MathF.Round(Math.Clamp(channel, 0.0f, 1.0f) * 255.0f);
    }
}
