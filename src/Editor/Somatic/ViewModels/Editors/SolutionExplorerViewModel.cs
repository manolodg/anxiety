using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Somatic.Controls.Model;

namespace Somatic.ViewModels {
    /// <summary>Navegación por las entidades de la escena.</summary>
    public partial class SolutionExplorerViewModel : Tool {
        [ObservableProperty] private EntityTreeNode? _rootEntity;
        [ObservableProperty] private EntityTreeNode? _selectedEntity;
    }
}
