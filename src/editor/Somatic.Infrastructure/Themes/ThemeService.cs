using Avalonia;
using Avalonia.Markup.Xaml.Styling;
using Microsoft.Extensions.Logging;
using Somatic.Core.Themes;

namespace Somatic.Infrastructure.Themes {
    public sealed class ThemeService : IThemeService {
        private const string DarkThemeUri  = "avares://Somatic.Themes/Dark.axaml";
        private const string LightThemeUri = "avares://Somatic.Themes/Light.axaml";

        private readonly ILogger<ThemeService> _logger;
        private ResourceInclude? _activeThemeResources;

        public ThemeService(ILogger<ThemeService> logger) {
            _logger = logger;
        }

        public ThemeVariant CurrentTheme { get; private set; } = ThemeVariant.Dark;

        public event EventHandler<ThemeVariant>? ThemeChanged;

        public void SetTheme(ThemeVariant theme) {
            Application? application = Application.Current ?? throw new InvalidOperationException("The Avalonia Application instance is not available yet.");

            Uri uri = new Uri(theme == ThemeVariant.Dark ? DarkThemeUri : LightThemeUri);
            ResourceInclude themeResources = new ResourceInclude(uri) { Source = uri };

            if (_activeThemeResources is not null) application.Resources.MergedDictionaries.Remove(_activeThemeResources);

            application.Resources.MergedDictionaries.Add(themeResources);
            _activeThemeResources = themeResources;
            CurrentTheme = theme;

            _logger.LogInformation("Tema del editor {Theme}", theme);
            ThemeChanged?.Invoke(this, theme);
        }
    }
}
