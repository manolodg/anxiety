using Avalonia;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.Input;
using Somatic.Controls.Model;
using Somatic.Model;
using Somatic.ViewModels;
using System.Linq;

namespace Somatic.Controls {
    /// <summary>Control para el control del arbol de entidades.</summary>
    public partial class SearchEntityTreeControl : SearchTreeControl {
#region Propiedades Avalonia
        /// <summary>Proyecto sobre el que se sacan los datos.</summary>
        public static readonly StyledProperty<Project?> ProjectProperty = AvaloniaProperty.Register<SearchEntityTreeControl, Project?>(nameof(Project));
#endregion

#region Propiedades
        /// <summary>Proyecto sobre el que se sacan los datos.</summary>
        public Project? Project {
            get => GetValue(ProjectProperty);
            set {
                SetValue(ProjectProperty, value);
                CreateRootEntity(value);
            }
        }
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public SearchEntityTreeControl() {
            InitializeComponent();

            if (Design.IsDesignMode) {
                DataContext = new SolutionExplorerViewModel {
                    Project = new Project {
                        ActiveScene = new Scene {
                            Name = "Escena"
                        }
                    }
                };
            }

            // Al cambio del DataContext se redibuja el arbol.
            this.DataContextChanged += (s, e) => {
                CreateRootEntity(GetSolutionExplorerViewModel()!.Project);
            };
            // Cuando cambia el nodo seleccioando lo actualizamos (esto fuerza el dibujado de la información).
            this.PropertyChanged += (s, e) => {
                if (e.Property == SelectedNodeProperty) {
                    GetSolutionExplorerViewModel()!.SelectedTreeNode = SelectedNode as EntityTreeNode;
                }
            };
        }
#endregion

#region Métodos
    #region Acciones
        /// <summary>Añade una nueva entidad al nodo indicado.</summary>
        /// <param name="node">Nodo sobre el que añadir la entidad.</param>
        [RelayCommand]
        private void AddEntity(EntityTreeNode node) {
            Entity? entity = GetSolutionExplorerViewModel()!.CreateEntity(node.Scene != null ? node.Scene.Entities : node.Entity!.Children);
            if (entity != null) {
                TreeNode? newnode = CreateChildren(node.Original!, entity);
                OnSearchTextChanged(SearchText);
                SelectNodeByOriginal(newnode!);
                if (SelectedNode != null) StartEditing(SelectedNode);
            } else {
                // TODO message de error.
            }
        }
    #endregion

        /// <summary>Conversión del DataContext al SolutionExplorerViewModel.</summary>
        private SolutionExplorerViewModel? GetSolutionExplorerViewModel() => DataContext as SolutionExplorerViewModel;

        /// <summary>Creación de la estructura desde el proyecto.</summary>
        /// <param name="project">Proyecto que proporciona los datos.</param>
        private void CreateRootEntity(Project? project) {
            EntityTreeNode tmp = new EntityTreeNode {
                Name = project!.ActiveScene.Name,
                IsExpanded = true,
                IsSelected = true,
                Scene = project.ActiveScene,
                Type = "Scene"
            };

            foreach (BaseEntity entity in project.ActiveScene.Entities) {
                CreateChildren(tmp, entity);
            }

            SelectedNode = RootNode = tmp;
        }
        /// <summary>Recursivamente se pueblan los nodos hijo.</summary>
        /// <param name="root">Nodo de arranque.</param>
        /// <param name="rootEntity">Entidad con los datos para el nodo.</param>
        private TreeNode? CreateChildren(TreeNode root, BaseEntity? rootEntity) {
            if (rootEntity == null) return null;

            EntityTreeNode child = new() {
                Name = rootEntity.Name,
                Entity = rootEntity,
                IsExpanded = true,
                IsSelected = false,
                Type = "Entity"
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
