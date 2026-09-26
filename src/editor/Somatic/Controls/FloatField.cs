using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using System.Globalization;

namespace Somatic.Controls {
    /// <summary>
    /// Caja de texto para un <see cref="float"/>. El valor se confirma con Intro o al perder el foco; Escape
    /// descarta lo escrito. Acepta tanto '.' como ',' como separador decimal.
    /// </summary>
    public class FloatField : TextBox {
        public static readonly StyledProperty<float> ValueProperty =
            AvaloniaProperty.Register<FloatField, float>(nameof(Value), defaultBindingMode: BindingMode.TwoWay);

        public static readonly StyledProperty<float> MinimumProperty =
            AvaloniaProperty.Register<FloatField, float>(nameof(Minimum), float.NegativeInfinity);

        public static readonly StyledProperty<float> MaximumProperty =
            AvaloniaProperty.Register<FloatField, float>(nameof(Maximum), float.PositiveInfinity);

        public float Value {
            get => GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        public float Minimum {
            get => GetValue(MinimumProperty);
            set => SetValue(MinimumProperty, value);
        }

        public float Maximum {
            get => GetValue(MaximumProperty);
            set => SetValue(MaximumProperty, value);
        }

        protected override Type StyleKeyOverride => typeof(TextBox);

        public FloatField() {
            Classes.Add("InspectorField");
            RefreshText();
        }

        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);
            if (change.Property == ValueProperty) RefreshText();
        }

        protected override void OnKeyDown(KeyEventArgs e) {
            switch (e.Key) {
                case Key.Enter:
                    Commit();
                    SelectAll();
                    e.Handled = true;
                    return;
                case Key.Escape:
                    RefreshText();
                    e.Handled = true;
                    return;
            }
            base.OnKeyDown(e);
        }

        protected override void OnLostFocus(FocusChangedEventArgs e) {
            Commit();
            base.OnLostFocus(e);
        }

        private void Commit() {
            string text = (Text ?? string.Empty).Trim().Replace(',', '.');
            if (float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out float parsed) && float.IsFinite(parsed)) {
                Value = Math.Clamp(parsed, Minimum, Maximum);
            }
            // Normaliza lo escrito (o lo revierte si no era un número válido).
            RefreshText();
        }

        private void RefreshText() => Text = Value.ToString("0.####", CultureInfo.InvariantCulture);
    }
}
