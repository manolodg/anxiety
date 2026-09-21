using Somatic.Core.Themes;

namespace Somatic.Core.Configuration {
    public interface IEditorConfiguration {
        ThemeVariant DefaultTheme { get; }

        string Language        { get; }
        double WindowWidth     { get; }
        double WindowHeight    { get; }
        double WindowMinWidth  { get; }
        double WindowMinHeight { get; }
    }
}
