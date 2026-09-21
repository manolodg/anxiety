using Dock.Model.Controls;
using Dock.Model.Core;
using Dock.Model.Mvvm;
using Dock.Model.Mvvm.Controls;
using Somatic.Core.Localization;
using Somatic.ViewModels.Documents;
using Somatic.ViewModels.Panels;

namespace Somatic.ViewModels.Docking {
    public sealed class MainLayoutFactory : Factory {
        private readonly ILocalizationService _localization;

        public MainLayoutFactory(ILocalizationService localization) {
            _localization = localization;
        }

        public override IRootDock CreateLayout() {
            HierarchyToolViewModel hierarchy = new HierarchyToolViewModel { Id = "Hierarchy", Title = _localization.GetString("Hierarchy") };
            InspectorToolViewModel inspector = new InspectorToolViewModel { Id = "Inspector", Title = _localization.GetString("Inspector") };
            ProjectToolViewModel   project   = new ProjectToolViewModel   { Id = "Project",   Title = _localization.GetString("Project") };
            ConsoleToolViewModel   console   = new ConsoleToolViewModel   { Id = "Console",   Title = _localization.GetString("Console"), EmptyStateText = _localization.GetString("NoMessages") };
            SceneDocumentViewModel scene     = new SceneDocumentViewModel { Id = "Scene",     Title = _localization.GetString("Scene"), CanClose = false };
            GameDocumentViewModel  game      = new GameDocumentViewModel  { Id = "Game",      Title = _localization.GetString("Game"),  CanClose = false };

            ToolDock leftDock = new ToolDock {
                Id               = "LeftPane",
                Proportion       = 0.18,
                Alignment        = Alignment.Left,
                ActiveDockable   = hierarchy,
                VisibleDockables = CreateList<IDockable>(hierarchy)
            };
            ToolDock rightDock = new ToolDock {
                Id               = "RightPane",
                Proportion       = 0.22,
                Alignment        = Alignment.Right,
                ActiveDockable   = inspector,
                VisibleDockables = CreateList<IDockable>(inspector),
            };

            DocumentDock mainDocuments = new DocumentDock {
                Id                = "MainArea",
                IsCollapsable     = false,
                CanCreateDocument = false,
                ActiveDockable    = scene,
                VisibleDockables  = CreateList<IDockable>(scene, game),
            };

            ProportionalDock centerRow = new ProportionalDock {
                Id               = "CenterRow",
                Orientation      = Orientation.Horizontal,
                VisibleDockables = CreateList<IDockable>(
                    leftDock,
                    new ProportionalDockSplitter(),
                    mainDocuments,
                    new ProportionalDockSplitter(),
                    rightDock),
            };

            ToolDock bottomDock = new ToolDock {
                Id               = "BottomPane",
                Proportion       = 0.25,
                Alignment        = Alignment.Bottom,
                ActiveDockable   = project,
                VisibleDockables = CreateList<IDockable>(project, console),
            };

            ProportionalDock mainLayout = new ProportionalDock {
                Id               = "MainLayout",
                Orientation      = Orientation.Vertical,
                VisibleDockables = CreateList<IDockable>(
                    centerRow,
                    new ProportionalDockSplitter(),
                    bottomDock),
            };

            IRootDock root = CreateRootDock();
            root.Id               = "Root";
            root.IsCollapsable    = false;
            root.ActiveDockable   = mainLayout;
            root.DefaultDockable  = mainLayout;
            root.VisibleDockables = CreateList<IDockable>(mainLayout);

            return root;
        }
    }
}
