using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Controls;
using Somatic.Core.Localization;
using Somatic.Core.Workspace;
using Somatic.ViewModels.Docking;

namespace Somatic.ViewModels {
    public partial class MainWindowViewModel : ObservableObject {
        private readonly MainLayoutFactory _factory;
        private readonly IWorkspaceLayoutService _workspaceLayoutService;

        [ObservableProperty] private IRootDock? _layout;

        public string ApplicationTitle      { get; }
        public string FileMenuText          { get; }
        public string EditMenuText          { get; }
        public string AssetsMenuText        { get; }
        public string WindowMenuText        { get; }
        public string HelpMenuText          { get; }
        public string ToolbarAccessibleName { get; }

        public MainWindowViewModel(MainLayoutFactory factory, ILocalizationService localization, IWorkspaceLayoutService workspaceLayoutService) {
            _factory = factory;
            _workspaceLayoutService = workspaceLayoutService;

            ApplicationTitle      = localization.GetString("ApplicationName");
            FileMenuText          = localization.GetString("File");
            EditMenuText          = localization.GetString("Edit");
            AssetsMenuText        = localization.GetString("Assets");
            WindowMenuText        = localization.GetString("Window");
            HelpMenuText          = localization.GetString("Help");
            ToolbarAccessibleName = localization.GetString("Toolbar");

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
    }
}
