using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Somatic.Controls;
using Somatic.Controls.Model;
using Somatic.Model;
using Somatic.Services;
using System.Collections.ObjectModel;

namespace Somatic.ViewModels {
    /// <summary>ViewModel de la ventana de información.</summary>
    public partial class InformationViewModel : Tool {
#region Propiedades
        /// <summary>Componente con el listado de componentes.</summary>
        public ComponentListControl? ComponentListControl { get; set; }

        /// <summary>Componentes que se gestionan.</summary>
        [ObservableProperty] private ObservableCollection<BaseComponent> _components = [];
        /// <summary>Indica el nodo seleccionado.</summary>
        [ObservableProperty] private EntityTreeNode? _node = null!;
#endregion

#region Campos
        /// <summary>Servicio de localización.</summary>
        private readonly ILocalizationService _localizationService = null!;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public InformationViewModel() { }
        /// <summary>Crea una instancia de la clase.</summary>
        public InformationViewModel(ILocalizationService localizationService) {
            _localizationService = localizationService;

            Title = _localizationService.GetString("Info_title");
        }
#endregion

#region Métodos
        /// <summary>Añade un nuevo componente.</summary>
        /// <param name="component">Componente a añadir.</param>
        public void AddComponent(BaseComponent component) => Components.Add(component);
        /// <summary>Elimina un componente.</summary>
        /// <param name="component">Componente a eliminar.</param>
        public void RemoveComponent(BaseComponent component) {
            if (Components.Remove(component)) OnPropertyChanged(nameof(Components));
        }
        /// <summary>Actualización del componente.</summary>
        public void UpdateComponent() => OnPropertyChanged(nameof(Components));
#endregion
    }
}
