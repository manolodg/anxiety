using Avalonia;
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
        }
#endregion

#region Métodos
        /// <summary>Creación de la estructura desde el proyecto.</summary>
        /// <param name="project">Proyecto que proporciona los datos.</param>
        private void CreateRootEntity(Project? project) {
            SelectedNode = RootNode = new EntityTreeNode {
                Name = project!.Name,
                IsExpanded = true,
                IsSelected = true,
                Scene = project.ActiveScene
            };

            foreach (BaseEntity entity in project.ActiveScene.Entities) {
                CreateChildren(RootNode, entity);
            }
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
                IsSelected = false
            };
            root.Children.Add(child);

            foreach (BaseEntity entity in rootEntity.Children) {
                CreateChildren(child, entity);
            }
        }
#endregion
    }
}
