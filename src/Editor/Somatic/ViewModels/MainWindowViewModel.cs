using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Controls;
using Dock.Model.Core;
using Somatic.Model;

namespace Somatic.ViewModels {
    public partial class MainWindowViewModel : ViewModelBase {
        [ObservableProperty] private IRootDock _layout;

        public MainWindowViewModel() {
            if (Design.IsDesignMode) {
                IFactory factory = new MainDockFactory(Framework.ServiceProvider);
                Layout = factory.CreateLayout()!;
                factory.InitLayout(Layout);
            }
        }
        public MainWindowViewModel(IRootDock layout) {
            Layout = layout;
        }
    }
}
