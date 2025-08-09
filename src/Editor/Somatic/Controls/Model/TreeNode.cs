using CommunityToolkit.Mvvm.ComponentModel;
using System.Collections.ObjectModel;

namespace Somatic.Controls.Model {
    /// <summary>Clase abstracta que representa la estructura y datos del TreeView.</summary>
    public abstract partial class TreeNode : ObservableObject {
#region Propiedades
        /// <summary>Nodos hijo que tiene este nodo.</summary>
        public ObservableCollection<TreeNode> Children { get; } = [];

        /// <summary>Nombre que aparece en el arbol.</summary>
        [ObservableProperty] private string _name = null!;
        /// <summary>Indica si el nodo esta o no expandido.</summary>
        [ObservableProperty] private bool _isExpanded = true;
        /// <summary>Indica si el nodo esta o no seleccionado.</summary>
        [ObservableProperty] private bool _isSelected = false;
        /// <summary>Indica si el nodo esta o no siendo editado (el nombre).</summary>
        [ObservableProperty] private bool _isEditing = false;
#endregion
    }
}
