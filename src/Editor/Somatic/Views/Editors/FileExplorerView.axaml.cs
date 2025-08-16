using Avalonia.Controls;

namespace Somatic.Views {
    public partial class FileExplorerView : UserControl {
        public FileExplorerView() {
            InitializeComponent();
        }

        private void OnDockControlPointerEntered(object? sender, Avalonia.Input.PointerEventArgs e) {
            if (sender is Control control && !control.IsFocused) control.Focus();
        }
    }
}
