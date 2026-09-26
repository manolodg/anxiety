using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Somatic.Core.Scene;
using Somatic.ViewModels.Panels;

namespace Somatic.Views.Panels {
    // HierarchyView --------------------------------------------------------------------------------
    // Interacción del árbol que no cabe en bindings:
    //
    //  - Arrastrar y soltar para reordenar y reparentar. La zona de la fila bajo el puntero decide el
    //    destino: franja superior = antes, inferior = después, centro = como hija. Soltar en el hueco
    //    libre del panel la manda al final de la raíz.
    //  - Renombrar en línea (F2 o menú contextual): el ViewModel lanza RenameRequested y aquí se muestra
    //    el TextBox de la fila. Intro o perder el foco confirma, Escape cancela.
    // ---------------------------------------------------------------------------------------------
    public partial class HierarchyView : UserControl {
        private enum DropPosition { Before, After, Into }

        private static readonly DataFormat<Entity> EntityFormat = DataFormat.CreateInProcessFormat<Entity>("Somatic.Entity");

        private const double DragThreshold = 4.0;
        private const double EdgeFraction  = 0.25;

        private HierarchyToolViewModel? _viewModel;

        private PointerPressedEventArgs? _pressArgs;
        private Point   _pressPoint;
        private Entity? _pressEntity;
        private bool    _dragging;
        private Border? _dropRow;

        private Entity?    _renameEntity;
        private TextBox?   _renameBox;
        private TextBlock? _renameLabel;

        public HierarchyView() {
            InitializeComponent();

            EntityTree.AddHandler(PointerPressedEvent,  OnTreePointerPressed,  RoutingStrategies.Tunnel | RoutingStrategies.Bubble, handledEventsToo: true);
            EntityTree.AddHandler(PointerMovedEvent,    OnTreePointerMoved,    RoutingStrategies.Tunnel | RoutingStrategies.Bubble, handledEventsToo: true);
            EntityTree.AddHandler(PointerReleasedEvent, OnTreePointerReleased, RoutingStrategies.Tunnel | RoutingStrategies.Bubble, handledEventsToo: true);

            DragDrop.AddDragOverHandler(EntityTree,  OnDragOver);
            DragDrop.AddDragLeaveHandler(EntityTree, OnDragLeave);
            DragDrop.AddDropHandler(EntityTree,      OnDrop);
        }

        protected override void OnDataContextChanged(EventArgs e) {
            base.OnDataContextChanged(e);

            if (_viewModel is not null) _viewModel.RenameRequested -= OnRenameRequested;
            _viewModel = DataContext as HierarchyToolViewModel;
            if (_viewModel is not null) _viewModel.RenameRequested += OnRenameRequested;
        }

        // --- Arrastrar ---------------------------------------------------------------------------

        private void OnTreePointerPressed(object? sender, PointerPressedEventArgs e) {
            if (e.Route == RoutingStrategies.Tunnel) return; // basta con verlo una vez; el túnel sólo asegura recibirlo
            if (!e.GetCurrentPoint(EntityTree).Properties.IsLeftButtonPressed) return;
            if ((e.Source as Visual)?.FindAncestorOfType<TextBox>(includeSelf: true) is not null) return;

            Border? row = FindRow(e.Source as Visual);
            _pressEntity = row?.DataContext as Entity;
            _pressArgs   = _pressEntity is null ? null : e;
            _pressPoint  = e.GetPosition(EntityTree);
        }

        private async void OnTreePointerMoved(object? sender, PointerEventArgs e) {
            if (e.Route == RoutingStrategies.Tunnel) return;
            if (_dragging || _pressArgs is null || _pressEntity is null) return;
            if (!e.GetCurrentPoint(EntityTree).Properties.IsLeftButtonPressed) {
                ResetPress();
                return;
            }

            Vector delta = e.GetPosition(EntityTree) - _pressPoint;
            if (Math.Abs(delta.X) < DragThreshold && Math.Abs(delta.Y) < DragThreshold) return;

            // El TreeViewItem pulsado captura el puntero; si la entidad cambia de padre ese contenedor se
            // destruye y la captura quedaría en un elemento fuera del árbol, tragándose los siguientes eventos.
            IPointer pointer = _pressArgs.Pointer;
            pointer.Capture(null);

            _dragging = true;
            try {
                DataTransfer data = new DataTransfer();
                data.Add(DataTransferItem.Create(EntityFormat, _pressEntity));
                await DragDrop.DoDragDropAsync(_pressArgs, data, DragDropEffects.Move);
            } finally {
                pointer.Capture(null);
                ClearDropIndicator();
                ResetPress();
            }
        }

        private void OnTreePointerReleased(object? sender, PointerReleasedEventArgs e) {
            if (!_dragging) ResetPress();
        }

        private void ResetPress() {
            _dragging    = false;
            _pressArgs   = null;
            _pressEntity = null;
        }

        // --- Soltar ------------------------------------------------------------------------------

        private void OnDragOver(object? sender, DragEventArgs e) {
            e.DragEffects = DragDropEffects.None;
            ClearDropIndicator();

            if (e.DataTransfer.TryGetValue(EntityFormat) is not { } dragged) return;
            if (!TryResolveDrop(e, dragged, out Border? row, out DropPosition position, out _, out _)) return;

            e.DragEffects = DragDropEffects.Move;
            if (row is null) return;

            _dropRow = row;
            row.Classes.Add(position switch {
                DropPosition.Before => "DropBefore",
                DropPosition.After  => "DropAfter",
                _                   => "DropInto"
            });
        }

        private void OnDragLeave(object? sender, DragEventArgs e) => ClearDropIndicator();

        private void OnDrop(object? sender, DragEventArgs e) {
            ClearDropIndicator();

            if (_viewModel is null || e.DataTransfer.TryGetValue(EntityFormat) is not { } dragged) return;
            if (!TryResolveDrop(e, dragged, out _, out _, out Entity? parent, out int index)) return;

            _viewModel.MoveEntity(dragged, parent, index);
            e.DragEffects = DragDropEffects.Move;
        }

        /// <summary>Traduce la posición del puntero a (padre, índice) de destino y dice si el movimiento es válido.</summary>
        private bool TryResolveDrop(DragEventArgs e, Entity dragged, out Border? row, out DropPosition position, out Entity? parent, out int index) {
            row      = FindRow(e.Source as Visual);
            position = DropPosition.Into;
            parent   = null;
            index    = -1;

            // Hueco libre del panel: al final de la raíz.
            if (row?.DataContext is not Entity target) return _viewModel is not null;
            if (target == dragged) return false;

            double height = Math.Max(1.0, row.Bounds.Height);
            double y = e.GetPosition(row).Y;
            position = y < height * EdgeFraction ? DropPosition.Before
                     : y > height * (1.0 - EdgeFraction) ? DropPosition.After
                     : DropPosition.Into;

            if (position == DropPosition.Into) {
                parent = target;
            } else if (position == DropPosition.After && target.IsExpanded && target.Children.Count > 0) {
                // Debajo de un padre desplegado lo que se ve a continuación es su primera hija.
                parent = target;
                index  = 0;
            } else {
                parent = target.Parent;
                IReadOnlyList<Entity> siblings = parent?.Children ?? (IReadOnlyList<Entity>?)_viewModel?.RootEntities ?? [];
                int targetIndex = IndexOf(siblings, target);
                index = position == DropPosition.Before ? targetIndex : targetIndex + 1;
            }

            return _viewModel is not null && _viewModel.CanMoveEntity(dragged, parent);
        }

        private void ClearDropIndicator() {
            _dropRow?.Classes.RemoveAll(["DropBefore", "DropAfter", "DropInto"]);
            _dropRow = null;
        }

        // --- Renombrar ---------------------------------------------------------------------------

        private void OnRenameRequested(object? sender, Entity entity) {
            EndRename(commit: true);

            if (EntityTree.TreeContainerFromItem(entity) is not TreeViewItem item) return;
            if (FindRowOf(item) is not { } row) return;

            List<Visual> parts = row.GetVisualDescendants().ToList();
            _renameBox   = parts.OfType<TextBox>().FirstOrDefault(c => c.Name == "RenameBox");
            _renameLabel = parts.OfType<TextBlock>().FirstOrDefault(c => c.Name == "NameText");
            if (_renameBox is null || _renameLabel is null) return;

            _renameEntity = entity;
            _renameBox.Text = entity.Name;
            _renameLabel.IsVisible = false;
            _renameBox.IsVisible   = true;
            _renameBox.KeyDown   += OnRenameKeyDown;
            _renameBox.LostFocus += OnRenameLostFocus;

            TextBox box = _renameBox;
            Dispatcher.UIThread.Post(() => {
                box.Focus();
                box.SelectAll();
            }, DispatcherPriority.Input);
        }

        private void OnRenameKeyDown(object? sender, KeyEventArgs e) {
            if (e.Key is not (Key.Enter or Key.Escape)) return;

            e.Handled = true;
            Entity? entity = _renameEntity;
            EndRename(commit: e.Key == Key.Enter);

            // Devuelve el foco al árbol para seguir navegando con el teclado.
            if (entity is not null && EntityTree.TreeContainerFromItem(entity) is TreeViewItem item) item.Focus();
        }

        private void OnRenameLostFocus(object? sender, FocusChangedEventArgs e) => EndRename(commit: true);

        private void EndRename(bool commit) {
            if (_renameEntity is null || _renameBox is null || _renameLabel is null) return;

            Entity    entity = _renameEntity;
            TextBox   box    = _renameBox;
            TextBlock label  = _renameLabel;
            _renameEntity = null;
            _renameBox    = null;
            _renameLabel  = null;

            box.KeyDown   -= OnRenameKeyDown;
            box.LostFocus -= OnRenameLostFocus;
            box.IsVisible   = false;
            label.IsVisible = true;

            if (commit) _viewModel?.RenameEntity(entity, box.Text ?? string.Empty);
        }

        // --- Utilidades --------------------------------------------------------------------------

        /// <summary>Fila (Border.EntityRow) del TreeViewItem que contiene al visual, aunque el puntero esté en la sangría o el desplegable.</summary>
        private static Border? FindRow(Visual? visual) {
            TreeViewItem? item = visual?.FindAncestorOfType<TreeViewItem>(includeSelf: true);
            return item is null ? null : FindRowOf(item);
        }

        // El TreeViewItem también contiene las filas de sus hijos; la suya es la que comparte DataContext.
        private static Border? FindRowOf(TreeViewItem item) =>
            item.GetVisualDescendants().OfType<Border>().FirstOrDefault(b => b.Classes.Contains("EntityRow") && b.DataContext == item.DataContext);

        private static int IndexOf(IReadOnlyList<Entity> list, Entity entity) {
            for (int i = 0; i < list.Count; ++i) {
                if (list[i] == entity) return i;
            }
            return -1;
        }
    }
}
