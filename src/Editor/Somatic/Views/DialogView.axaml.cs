using Avalonia.Controls;
using System;

namespace Somatic.Views {
    public partial class DialogView : Window {
        public readonly Action? _mainAction;

        public DialogView() { }
        public DialogView(Action? mainAction) {
            InitializeComponent();

            _mainAction = mainAction;
        }
        private void CloseButton_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
            Close();
        }
    }
}
