using Dock.Model.Controls;

namespace Somatic.Core.Workspace {
    public interface IWorkspaceLayoutService {
        IRootDock? TryLoadLayout();
        void SaveLayout(IRootDock layout);
        void ResetLayout();
    }
}
