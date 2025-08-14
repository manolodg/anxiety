using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Somatic.Controls.Model;
using Somatic.Model;
using Somatic.Services;
using System.Collections.ObjectModel;

namespace Somatic.ViewModels {
    /// <summary>Navegación por las entidades de la escena.</summary>
    public partial class SolutionExplorerViewModel : Tool {
#region Propiedades
        [ObservableProperty] private Project _project = null!;
        [ObservableProperty] private EntityTreeNode? _selectedTreeNode = null!;
#endregion

#region Campos
        private readonly InformationViewModel _informationViewModel = null!;
        private readonly ILocalizationService _localizationService = null!;
        private readonly IEntityService _entityService = null!;
#endregion

#region Constructores
        /// <summary>Crea una instancia ficticia para el editor.</summary>
        public SolutionExplorerViewModel() {
            if (Design.IsDesignMode) {
                Project = new MockupProject();
            }
        }
        /// <summary>Crea una nueva instancia de la clase.</summary>
        /// <param name="project">Proyecto inyectado en el sistema.</param>
        /// <param name="informationViewModel">ModelView de la ventana Information.</param>
        /// <param name="localizationService">Servicio de traducción.</param>
        /// <param name="entityService">Servicio de entidades.</param>
        public SolutionExplorerViewModel(
                Project project, 
                InformationViewModel informationViewModel,
                ILocalizationService localizationService,
                IEntityService entityService) {
            Project = project;
            _informationViewModel = informationViewModel;
            _localizationService = localizationService;
            _entityService = entityService;

            PropertyChanged += (s, e) => {
                if (e.PropertyName == nameof(SelectedTreeNode)) {
                    if (SelectedTreeNode != null && SelectedTreeNode?.Scene == null) {
                        _informationViewModel.Components = SelectedTreeNode!.Entity!.Components;
                        _informationViewModel.Node = SelectedTreeNode;
                    }
                }
            };
        }
#endregion

#region Métodos
        /// <summary>Realiza la creación de una nueva entidad.</summary>
        /// <param name="entities">Colección sobre la que colgar la nueva entidad.</param>
        /// <returns>Nueva entidad generada.</returns>
        public Entity? CreateEntity(ObservableCollection<BaseEntity> entities) {
            return _entityService.CreateEntity(entities, ConvertToValidTitle(_localizationService.GetString("NewEntity")));
        }

        /// <summary>Busca en el arbol de entidades para dar un valor de nombre válido.</summary>
        /// <param name="name">Cadena de nombre (comienzo) a buscar.</param>
        /// <returns>Nombre válido a utilizar.</returns>
        private string ConvertToValidTitle(string name) {
            if (Project?.ActiveScene != null) {
                int value = -1;
                foreach (BaseEntity entity in Project.ActiveScene.Entities) {
                    value = RecursiveSearchTitle(entity, name, value);
                }

                if (value >= 0) name = $"{name} {++value}";
            }

            return name;

            // Realiza la búsqueda de un valor superior del título
            int RecursiveSearchTitle(BaseEntity entity, string title, int value) {
                foreach (BaseEntity child in entity.Children) {
                    value = RecursiveSearchTitle(child, title, value);
                }

                if (entity.Name.StartsWith(title)) {
                    string numero = entity.Name.Substring(title.Length);
                    if (!string.IsNullOrEmpty(numero)) {
                        int val = int.Parse(numero);
                        if (val > value) value = val;
                    } else if (value < 0) {
                        value = 0;
                    }
                }

                return value;
            }
        }
#endregion
    }
}
