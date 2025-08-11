using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Somatic.Controls.Model;
using Somatic.Model;
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
                EntityTreeNode root = new EntityTreeNode {
                    Name = "Raíz",
                    Type = "Scene"
                };
                EntityTreeNode node1 = new EntityTreeNode {
                    Name = "Nodo 1",
                    Type = "Entity"
                };
                EntityTreeNode node2 = new EntityTreeNode {
                    Name = "Nodo 2",
                    Type = "Entity"
                };
                root.Children.Add(node1);
                root.Children.Add(node2);

                EntityTreeNode node11 = new EntityTreeNode {
                    Name = "Nodo 11",
                    Type = "Entity"
                };
                node1.Children.Add(node11);

                RootNode = root;
            }
        }
#endregion

#region Métodos
        /// <summary>Creación de la estructura desde el proyecto.</summary>
        /// <param name="project">Proyecto que proporciona los datos.</param>
        private void CreateRootEntity(Project? project) {
            EntityTreeNode tmp = new EntityTreeNode {
                Name = project!.Name,
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
