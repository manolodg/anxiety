using Avalonia;
using Avalonia.Controls;
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

                var entities = (DataContext as SolutionExplorerViewModel)!.Project.ActiveScene.Entities;
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

            this.DataContextChanged += (s, e) => {
                CreateRootEntity((DataContext as SolutionExplorerViewModel)!.Project);
            };
            this.PropertyChanged += (s, e) => {
                if (e.Property == SelectedNodeProperty) {
                    (DataContext as SolutionExplorerViewModel)!.SelectedTreeNode = SelectedNode as EntityTreeNode;
                }
            };
        }
#endregion

#region Métodos
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
        private void CreateChildren(TreeNode root, BaseEntity? rootEntity) {
            if (rootEntity == null) return;

            EntityTreeNode child = new() {
                Name = rootEntity.Name,
                Entity = rootEntity,
                IsExpanded = rootEntity.Children.Any(),
                IsSelected = false,
                Type = "Entity"
            };
            root.Children.Add(child);

            foreach (BaseEntity entity in rootEntity.Children) {
                CreateChildren(child, entity);
            }
        }
#endregion
    }
}
