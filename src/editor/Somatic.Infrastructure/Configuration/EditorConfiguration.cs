using Microsoft.Extensions.Options;
using Somatic.Core.Configuration;
using Somatic.Core.Themes;

namespace Somatic.Infrastructure.Configuration {
    public sealed class EditorConfiguration : IEditorConfiguration {
        private readonly EditorOptions _options;

        public EditorConfiguration(IOptions<EditorOptions> options) {
            _options = options.Value;
        }

        public ThemeVariant DefaultTheme => Enum.TryParse<ThemeVariant>(_options.Theme.Default, ignoreCase: true, out var theme) ? theme : ThemeVariant.Dark;

        public string Language        => _options.Language;
        public double WindowWidth     => _options.Window.Width;
        public double WindowHeight    => _options.Window.Height;
        public double WindowMinWidth  => _options.Window.MinWidth;
        public double WindowMinHeight => _options.Window.MinHeight;
    }
}
