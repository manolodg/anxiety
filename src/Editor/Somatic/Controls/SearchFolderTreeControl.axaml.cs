using Avalonia;
using Avalonia.Controls;
using Somatic.Controls.Model;
using Somatic.Model;
using Somatic.ViewModels;

namespace Somatic.Controls {
    /// <summary>Control para el arbol de carpetas.</summary>
    public partial class SearchFolderTreeControl : UserControl {
#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public SearchFolderTreeControl() {
            InitializeComponent();

            // Al cambio del DataContext se redibuja el arbol.
            this.DataContextChanged += (s, e) => {
                CreateRootFolder(GetFileExplorerViewModel()!.RootPath);
            };
        }
#endregion

#region Métodos
    #region Acciones
    #endregion

        /// <summary>Conversión del DataContext al FileExplorerViewModel.</summary>
        /// <returns></returns>
        private FileExplorerViewModel? GetFileExplorerViewModel() => DataContext as FileExplorerViewModel;

        /// <summary>Creación de la estructura desde el proyecto.</summary>
        /// <param name="rootPath">Ruta del proyecto.</param>
        private void CreateRootFolder(string rootPath) {
            FolderTreeNode tmp = new FolderTreeNode {
                Name = rootPath,
                IsExpanded = true,
                IsSelected = true
            };
        }
        /// <summary>Recursivamente se pueblan los nodos hijo.</summary>
        /// <param name="root">Nodo de arranque.</param>
        /// <param name="rootEntity">Entidad con los datos para el nodo.</param>
        private TreeNode? CreateChildren(TreeNode root, BaseEntity? rootEntity) {
            if (rootEntity == null) return null;

            FolderTreeNode child = new() {
                Name = rootEntity.Name,
                IsExpanded = true,
                IsSelected = false
            };
            root.Children.Add(child);

            foreach (BaseEntity entity in rootEntity.Children) {
                CreateChildren(child, entity);
            }

            return child;
        }
#endregion
    }
}
