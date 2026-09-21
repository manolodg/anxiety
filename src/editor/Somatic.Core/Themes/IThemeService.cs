namespace Somatic.Core.Themes {
    public interface IThemeService {
        ThemeVariant CurrentTheme { get; }

        event EventHandler<ThemeVariant>? ThemeChanged;

        void SetTheme(ThemeVariant theme);
    }
}
