using Dock.Model.Controls;

namespace Somatic.Infrastructure.Workspace {
    internal sealed class WorkspaceLayoutEnvelope {
        public int Version { get; set; }
        public IRootDock? Layout { get; set; }
    }
}
