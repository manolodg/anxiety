using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.Controls.Model {
    /// <summary>Nodo especifico para el arbol de carpetas.</summary>
    public partial class FolderTreeNode : TreeNode {
#region Propiedades
        /// <summary>Ruta completa.</summary>
        [ObservableProperty] private string _fullPath = null!;
        /// <summary>Archivos contenidos en la carpeta.</summary>
        [ObservableProperty] private FileItem[] _files = [];
#endregion

#region Métodos
        /// <summary>Clona este objeto como <see cref="TreeNode"/>.</summary>
        public override TreeNode Clone() {
            return new FolderTreeNode {
                Name = this.Name,
                IsExpanded = this.IsExpanded,
                IsSelected = false,
                IsEditing = false,
                FullPath = this.FullPath,
                Type = this.Type,
                Original = this
            };
        }
#endregion
    }
}
