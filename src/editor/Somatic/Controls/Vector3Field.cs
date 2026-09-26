using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Layout;
using Avalonia.Media;
using System.Numerics;

namespace Somatic.Controls {
    /// <summary>Tres <see cref="FloatField"/> (X, Y, Z) editando un <see cref="Vector3"/>, con el color de eje habitual.</summary>
    public class Vector3Field : UserControl {
        public static readonly StyledProperty<Vector3> ValueProperty =
            AvaloniaProperty.Register<Vector3Field, Vector3>(nameof(Value), defaultBindingMode: BindingMode.TwoWay);

        private static readonly IBrush AxisXBrush = new SolidColorBrush(Color.FromRgb(0xE0, 0x52, 0x52));
        private static readonly IBrush AxisYBrush = new SolidColorBrush(Color.FromRgb(0x6C, 0xC0, 0x4A));
        private static readonly IBrush AxisZBrush = new SolidColorBrush(Color.FromRgb(0x3A, 0x96, 0xDD));

        private readonly FloatField _x = new();
        private readonly FloatField _y = new();
        private readonly FloatField _z = new();
        private bool _syncing;

        public Vector3 Value {
            get => GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        public Vector3Field() {
            Grid grid = new Grid { ColumnDefinitions = new ColumnDefinitions("*,4,*,4,*") };
            grid.Children.Add(CreateAxis("X", AxisXBrush, _x, 0));
            grid.Children.Add(CreateAxis("Y", AxisYBrush, _y, 2));
            grid.Children.Add(CreateAxis("Z", AxisZBrush, _z, 4));
            Content = grid;

            SyncFields();
        }

        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);
            if (change.Property == ValueProperty) SyncFields();
        }

        private Control CreateAxis(string label, IBrush brush, FloatField field, int column) {
            field.PropertyChanged += (_, e) => {
                if (e.Property == FloatField.ValueProperty && !_syncing) Value = new Vector3(_x.Value, _y.Value, _z.Value);
            };

            DockPanel axis = new DockPanel();
            Border marker = new Border { Width = 3, Background = brush, Margin = new Thickness(0, 2, 3, 2), VerticalAlignment = VerticalAlignment.Stretch };
            ToolTip.SetTip(marker, label);
            DockPanel.SetDock(marker, Avalonia.Controls.Dock.Left);
            axis.Children.Add(marker);
            axis.Children.Add(field);

            Grid.SetColumn(axis, column);
            return axis;
        }

        private void SyncFields() {
            _syncing = true;
            try {
                Vector3 value = Value;
                _x.Value = value.X;
                _y.Value = value.Y;
                _z.Value = value.Z;
            } finally {
                _syncing = false;
            }
        }
    }
}
