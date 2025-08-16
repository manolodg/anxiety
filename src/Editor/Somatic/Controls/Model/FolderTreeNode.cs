using CommunityToolkit.Mvvm.ComponentModel;
using Somatic.Model;
using System;

namespace Somatic.Controls.Model {
    /// <summary>Nodo especifico para el arbol de carpetas.</summary>
    public partial class FolderTreeNode : TreeNode {
        [ObservableProperty] private string _fullPath = null!;

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
