using Somatic.Model;
using System.ComponentModel;

namespace Somatic.Controls.Model {
    /// <summary>Nodo especifico para el arbol de entidades.</summary>
    public partial class EntityTreeNode : TreeNode {
#region Propiedades
        /// <summary>Entidad que contiene el TreeNode.</summary>
        public BaseEntity? Entity { get; set; }
        /// <summary>Escena que contiene el TreeNode.</summary>
        public Scene? Scene { get; set; }
#endregion

#region Métodos
        /// <summary>Clona este objeto como <see cref="TreeNode"/>.</summary>
        /// <returns>Nodo clonado.</returns>
        public override TreeNode Clone() {
            return new EntityTreeNode {
                Name = this.Name,
                IsExpanded = this.IsExpanded,
                IsSelected = false,
                IsEditing = false,
                Entity = this.Entity,
                Scene = this.Scene,
                Type = this.Type,
                Original = this
            };
        }

        /// <summary>Se produce cuando cambia cualquier propiedad.</summary>
        protected override void OnPropertyChanged(PropertyChangedEventArgs e) {
            if (e.PropertyName == nameof(Name) && Entity != null) Entity.Name = Name;

            base.OnPropertyChanged(e);
        }
#endregion
    }
}
