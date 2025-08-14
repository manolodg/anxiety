using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Controls.Model;
using Somatic.Models;
using Somatic.Services;
using System.Linq;

namespace Somatic.Controls {
    /// <summary>Control base que utilizaremos para crear todos los arboles con b�squeda.</summary>
    public partial class SearchTreeControl : UserControl {
#region Propiedades Avalonia
        /// <summary>Texto que se utiliza de filtro en la b�squeda.</summary>
        public static readonly StyledProperty<string> SearchTextProperty = AvaloniaProperty.Register<SearchTreeControl, string>(nameof(SearchText), string.Empty);
        /// <summary>Texto que se utiliza como Watermark.</summary>
        public static readonly StyledProperty<string> SearchWatermarkProperty = AvaloniaProperty.Register<SearchTreeControl, string>(nameof(SearchWatermark), Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("search") ?? "Buscar...");

        /// <summary>Nodo ra�z del arbol (objetos originales).</summary>
        public static readonly StyledProperty<TreeNode?> RootNodeProperty = AvaloniaProperty.Register<SearchTreeControl, TreeNode?>(nameof(RootNode));
        /// <summary>Nodo actualmente seleccionado del arbol.</summary>
        public static readonly StyledProperty<TreeNode?> SelectedNodeProperty = AvaloniaProperty.Register<SearchTreeControl, TreeNode?>(nameof(SelectedNode));

        /// <summary>Nodo buscado seg�n el filtro de b�squeda.</summary>
        public static readonly StyledProperty<TreeNode?> SearchedNodeProperty = AvaloniaProperty.Register<SearchTreeControl, TreeNode?>(nameof(SearchedNode));
#endregion

#region Propiedades
        /// <summary>Texto que se utiliza de filtro en la b�squeda.</summary>
        public string SearchText {
            get => GetValue(SearchTextProperty);
            set => SetValue(SearchTextProperty, value);
        }
        /// <summary>Texto que se utiliza como Watermark.</summary>
        public string SearchWatermark {
            get => GetValue(SearchWatermarkProperty);
            set => SetValue(SearchWatermarkProperty, value);
        }

        /// <summary>Nodo ra�z del arbol.</summary>
        public TreeNode? RootNode {
            get => GetValue(RootNodeProperty);
            set => SetValue(RootNodeProperty, value);
        }
        /// <summary>Nodo actualmente seleccionado del arbol.</summary>
        public TreeNode? SelectedNode {
            get => GetValue(SelectedNodeProperty);
            set => SetValue(SelectedNodeProperty, value);
        }

        /// <summary>Nodo buscado seg�n el filtro de b�squeda.</summary>
        public TreeNode? SearchedNode {
            get => GetValue(SearchedNodeProperty);
            set => SetValue(SearchedNodeProperty, value);
        }
#endregion

#region Constructor
        /// <summary>Crea una instancia de la clase <see cref="SearchTreeControl"/>.</summary>
        public SearchTreeControl() {
            InitializeComponent();

            WeakReferenceMessenger.Default.Register<NewEntityMessage>(this, (s, e) => {
                // Enfocar el TextBox de edici�n despues de un peque�o delay
                Dispatcher.UIThread.Post(() => { FocusEditingTextBox(); }, DispatcherPriority.Background);
            });
        }
#endregion

#region M�todos
    #region Comandos
        /// <summary>Maneja la selecci�n de un nodo seleccionandolo.</summary>
        /// <param name="node">Nodo a seleccionar</param>
        [RelayCommand]
        protected void NodeSelected(TreeNode node) {
            if (SelectedNode != null) SelectedNode.IsEditing = false;

            foreach (TreeNode child in SearchedNode!.Children) {
                RecursiveDeselection(child);
            }

            node.IsSelected = true;
            SelectedNode = node;
        }
        /// <summary>Inicia el modo de edici�n para el nodo seleccionado.</summary>
        /// <param name="node"><see cref="TreeNode"/> a poner en modo de edici�n.</param>
        [RelayCommand]
        protected void StartEditing(TreeNode node) {
            foreach (TreeNode child in SearchedNode!.Children) {
                StopEditingRecursive(child);
            }

            node.IsEditing = true; ;
            SelectedNode = node;
            Dispatcher.UIThread.Post(() => { FocusEditingTextBox(); }, DispatcherPriority.Background);
        }
        /// <summary>Para el modo de edici�n del nodo especificado.</summary>
        /// <param name="node">Nodo sobre el que hemos de parar el modo de edici�n.</param>
        [RelayCommand]
        protected void StopEditing(TreeNode node) {
            node.IsEditing = false;
            NodeSelected(node);
        }
    #endregion

    #region M�todos
        /// <summary>Se realiza el filtrado seg�n cabien las propiedades.</summary>
        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);

            if (change.Property == SearchTextProperty) {
                OnSearchTextChanged(change.NewValue as string ?? string.Empty);
            } else if (change.Property == RootNodeProperty) {
                OnSearchTextChanged(string.Empty);
            }
        }

        /// <summary>Realiza la busqueda de la cadena dentro de las entidades.</summary>
        protected void OnSearchTextChanged(string searchText) {
            if (RootNode == null) return;

            SearchedNode = RecursiveSearch(RootNode, searchText.ToLower());

            TreeNode? RecursiveSearch(TreeNode node, string search) {
                TreeNode? cloned = null;

                if (string.IsNullOrEmpty(search) || node.Name.ToLower().Contains(search)) cloned = node.Clone();

                foreach (TreeNode child in node.Children) {
                    TreeNode? clonedChild = RecursiveSearch(child, search);
                    if (clonedChild != null) {
                        if (cloned == null) cloned = node.Clone();
                        cloned.Children.Add(clonedChild);
                    }
                }

                return cloned;
            }
        }
        /// <summary>Realiza la b�squeda de un nodo mediante el original.</summary>
        /// <param name="original">Nodo original sobre el que buscar.</param>
        protected void SelectNodeByOriginal(TreeNode original) {
            SelectedNode = SearchRecurvise(SearchedNode!, original);

            TreeNode? SearchRecurvise(TreeNode from, TreeNode search) {
                foreach (TreeNode child in from.Children) {
                    if (child.Original == search) return child;
                    TreeNode? result = SearchRecurvise(child, search);
                    if (result != null) return result;
                }

                return null;
            }
        }

        /// <summary>Deselecciona el nodo especificado y todos sus descendentes.</summary>
        /// <param name="node">Nodo desde el que comenzamos la deselecci�n.</param>
        private void RecursiveDeselection(TreeNode node) {
            node.IsSelected = false;
            foreach (TreeNode child in node.Children) {
                RecursiveDeselection(child);
            }
        }
        /// <summary>Quita la edici�n en todos los nodos descendentes.</summary>
        /// <param name="node">Nodo ra�z desde el que comenzamos.</param>
        private void StopEditingRecursive(TreeNode node) {
            node.IsEditing = false;
            foreach (TreeNode child in node.Children) {
                StopEditingRecursive(child);
            }
        }
        /// <summary>Activa el foco a la edici�n activa y selecciona todo el texto.</summary>
        private void FocusEditingTextBox() {
            TextBox? textBox = FindEditingTextBox();
            if (textBox != null) {
                textBox.Focus();
                textBox.SelectAll();
            }
        }
        /// <summary>Encuentra el <see cref="TextBox"/> actualmente visible usado para la edici�n.</summary>
        /// <returns><see cref="TextBox"/> usado para la edici�n.</returns>
        private TextBox? FindEditingTextBox() {
            // Buscar usando el m�todo de extensi�n de Avalonia
            return MainTreeView.GetVisualDescendants()
                .OfType<TextBox>()
                .FirstOrDefault(tb => tb.Name == "EditTextBox" && tb.IsVisible);
        }
        /// <summary>Maneja la pulsaci�n del puntero en un nodo.</summary>
        private void OnNodePointerPressed(object? sender, PointerPressedEventArgs e) {
            if (sender is TextBlock textBlock && textBlock.DataContext is TreeNode node) {
                NodeSelected(node);
            }
        }
        /// <summary>Maneja el doble click en un nodo e inicia la edici�n.</summary>
        private void OnNodeDoubleTapped(object? sender, RoutedEventArgs e) {
            if (sender is TextBlock textBlock && textBlock.DataContext is TreeNode node) {
                StartEditing(node);
                Dispatcher.UIThread.Post(() => { FocusEditingTextBox(); }, DispatcherPriority.Background);
            }
        }
        /// <summary>Maneja el evento de perdida de foco y cierra la edici�n.</summary>
        private void OnEditTextBoxLostFocus(object? sender, RoutedEventArgs e) {
            if (sender is TextBox textBox && textBox.DataContext is TreeNode node) {
                StopEditing(node);
            }
        }
        /// <summary>Quitamos la edici�n por si est� marcada ahora mismo.</summary>
        private void OnSearchBoxGotFocus(object? sender, RoutedEventArgs e) {
            if (SelectedNode != null) StopEditing(SelectedNode);
        }
    #endregion
#endregion
    }
}
