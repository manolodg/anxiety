using Somatic.Controls.Model;
using Somatic.ViewModels;
using System;
using System.IO;
using System.Linq;

namespace Somatic.Controls {
    /// <summary>Control para el arbol de carpetas.</summary>
    public partial class SearchFolderTreeControl : SearchTreeControl {
#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public SearchFolderTreeControl() {
            InitializeComponent();

            // Al cambio del DataContext se redibuja el arbol.
            this.DataContextChanged += (s, e) => {
                CreateRootFolder(GetFileExplorerViewModel()!.RootPath);
            };
            this.PropertyChanged += (s, e) => {
                if (e.Property == SelectedNodeProperty && e.NewValue != null) {
                    GetFileExplorerViewModel()!.SelectedPath = ((FolderTreeNode)SelectedNode).FullPath;
                    GetFileExplorerViewModel()!.LoadFolderContents();
                }
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
            try {
                if (!Directory.Exists(rootPath)) return;

                FolderTreeNode root = new FolderTreeNode {
                    Name = Path.GetFileName(rootPath.Substring(0, rootPath.Length - 1)),
                    IsExpanded = true,
                    FullPath = rootPath
                };

                CreateChildren(root);

                SelectedNode = RootNode = root;
            } catch (Exception ex) {
                // TODO: errro
            }
        }
        /// <summary>Recursivamente se pueblan los nodos hijo.</summary>
        /// <param name="root">Nodo de arranque.</param>
        private void CreateChildren(FolderTreeNode root) {
            try {
                string[] directories = Directory.GetDirectories(root.FullPath);
                foreach (string directory in directories.Where(x => !x.EndsWith(".soma"))) {
                    FolderTreeNode folder = new FolderTreeNode {
                        Name = Path.GetFileName(directory),
                        FullPath = directory
                    };

                    if (Directory.GetDirectories(directory).Length > 0) CreateChildren(folder);

                    root.Children.Add(folder);
                }
            } catch (UnauthorizedAccessException) {
            } catch (Exception ex) {
                // TODO: errro
            }
        }
#endregion
    }
}
