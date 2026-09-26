using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Somatic.Core.History;
using Somatic.Core.Localization;
using Somatic.Core.Workspace;
using Somatic.ViewModels.Docking;
using System.ComponentModel;

namespace Somatic.ViewModels {
    public partial class MainWindowViewModel : ObservableObject {
        private readonly MainLayoutFactory _factory;
        private readonly IWorkspaceLayoutService _workspaceLayoutService;
        private readonly IUndoService _undo;
        private readonly string _undoText;
        private readonly string _redoText;

        [ObservableProperty] private IRootDock? _layout;

        public string ApplicationTitle      { get; }
        public string FileMenuText          { get; }
        public string EditMenuText          { get; }
        public string AssetsMenuText        { get; }
        public string WindowMenuText        { get; }
        public string HelpMenuText          { get; }
        public string ToolbarAccessibleName { get; }

        /// <summary>"Deshacer: Mover «Luz»", o sólo "Deshacer" si no hay nada que deshacer.</summary>
        public string UndoHeader => _undo.UndoDescription is { } description ? $"{_undoText}: {description}" : _undoText;
        public string RedoHeader => _undo.RedoDescription is { } description ? $"{_redoText}: {description}" : _redoText;

        public IRelayCommand UndoCommand { get; }
        public IRelayCommand RedoCommand { get; }

        public MainWindowViewModel(MainLayoutFactory factory, ILocalizationService localization, IWorkspaceLayoutService workspaceLayoutService, IUndoService undo) {
            _factory = factory;
            _workspaceLayoutService = workspaceLayoutService;
            _undo = undo;

            ApplicationTitle      = localization.GetString("ApplicationName");
            FileMenuText          = localization.GetString("File");
            EditMenuText          = localization.GetString("Edit");
            AssetsMenuText        = localization.GetString("Assets");
            WindowMenuText        = localization.GetString("Window");
            HelpMenuText          = localization.GetString("Help");
            ToolbarAccessibleName = localization.GetString("Toolbar");
            _undoText             = localization.GetString("Undo");
            _redoText             = localization.GetString("Redo");

            UndoCommand = new RelayCommand(_undo.Undo, () => _undo.CanUndo);
            RedoCommand = new RelayCommand(_undo.Redo, () => _undo.CanRedo);
            _undo.PropertyChanged += OnUndoPropertyChanged;

            Layout = _workspaceLayoutService.TryLoadLayout() ?? _factory.CreateLayout();
            if (Layout is not null) factory.InitLayout(Layout);
        }

        public void SaveWorkspaceLayout() {
            if (Layout is not null) _workspaceLayoutService.SaveLayout(Layout);
        }

        public void ResetWorkspaceLayout() {
            _workspaceLayoutService.ResetLayout();

            Layout = _factory.CreateLayout();
            if (Layout is not null) _factory.InitLayout(Layout);
        }

        private void OnUndoPropertyChanged(object? sender, PropertyChangedEventArgs e) {
            switch (e.PropertyName) {
                case nameof(IUndoService.CanUndo):
                    UndoCommand.NotifyCanExecuteChanged();
                    break;
                case nameof(IUndoService.CanRedo):
                    RedoCommand.NotifyCanExecuteChanged();
                    break;
                case nameof(IUndoService.UndoDescription):
                    OnPropertyChanged(nameof(UndoHeader));
                    break;
                case nameof(IUndoService.RedoDescription):
                    OnPropertyChanged(nameof(RedoHeader));
                    break;
            }
        }
    }
}
