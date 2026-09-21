using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Dock.Model.Core;
using Somatic.ViewModels.Documents;
using Somatic.ViewModels.Panels;
using Somatic.Views.Documents;
using Somatic.Views.Panels;

namespace Somatic {
    public sealed class ViewLocator : IDataTemplate {
        private static readonly Dictionary<Type, Func<Control>> Views = new() {
            [typeof(HierarchyToolViewModel)] = () => new HierarchyView(),
            [typeof(InspectorToolViewModel)] = () => new InspectorView(),
            [typeof(ProjectToolViewModel)]   = () => new ProjectView(),
            [typeof(ConsoleToolViewModel)]   = () => new ConsoleView(),
            [typeof(SceneDocumentViewModel)] = () => new SceneView(),
            [typeof(GameDocumentViewModel)]  = () => new GameView()
        };

        public Control? Build(object? data) {
            if (data is not null && Views.TryGetValue(data.GetType(), out Func<Control>? factory)) return factory();
            return new TextBlock { Text = $"No hay vista registrada para {data?.GetType().Name}" };
        }

        public bool Match(object? data) => data is IDockable;
    }
}
