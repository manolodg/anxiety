using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Somatic.Controls.Model;
using Somatic.Model;

namespace Somatic.ViewModels {
    /// <summary>Navegación por las entidades de la escena.</summary>
    public partial class SolutionExplorerViewModel : Tool {
        private readonly InformationViewModel _informationViewModel;

        [ObservableProperty] private Project _project = null!;
        [ObservableProperty] private EntityTreeNode? _selectedTreeNode = null!;

        public SolutionExplorerViewModel() {
            if (Design.IsDesignMode) {
                Project = new Project {
                    ActiveScene = new Scene {
                        Name = "Escena"
                    }
                };
                var entities = Project.ActiveScene.Entities;
                var entity1 = new Entity { Name = "Entidad raíz 1" };
                var entity11 = new Entity { Name = "Entidad hija 1 de 1" };
                var entity12 = new Entity { Name = "Entidad hija 2 de 1" };
                entity1.Children.Add(entity11);
                entity1.Children.Add(entity12);

                var entity2 = new Entity { Name = "Entidad raíz 2" };
                var entity21 = new Entity { Name = "Entidad hija 1 de 2" };
                entity2.Children.Add(entity21);

                var entity211 = new Entity { Name = "Entidad nieta 1 de 2" };
                entity21.Children.Add(entity211);

                entities.Add(entity1);
                entities.Add(entity2);
            }
        }

        public SolutionExplorerViewModel(Project project, InformationViewModel informationViewModel) {
            Project = project;
            _informationViewModel = informationViewModel;

            PropertyChanged += (s, e) => {
                if (e.PropertyName == nameof(SelectedTreeNode)) {
                    if (SelectedTreeNode != null && SelectedTreeNode?.Scene == null) {
                        _informationViewModel.Components = SelectedTreeNode!.Entity!.Components;
                    }
                }
            };
        }
    }
}
