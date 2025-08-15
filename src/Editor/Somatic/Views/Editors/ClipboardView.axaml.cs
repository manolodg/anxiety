using Avalonia.Controls;

namespace Somatic.Views;

public partial class ClipboardView : UserControl {
    public ClipboardView() {
        InitializeComponent();
    }

    private void OnDockControlPointerEntered(object? sender, Avalonia.Input.PointerEventArgs e) {
        if (sender is Control control && !control.IsFocused) control.Focus();
    }
}