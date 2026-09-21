using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Somatic.Core.Configuration;
using Somatic.Core.Themes;
using Somatic.ViewModels;
using Somatic.Views;

namespace Somatic;

public partial class App : Application {
    public static IServiceProvider? Services { get; set; }

    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted() {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) {
            IServiceProvider services = Services ?? throw new InvalidOperationException("No se ha configurado Services.");
            ILogger<App> logger = services.GetRequiredService<ILogger<App>>();

            IThemeService themeService = services.GetRequiredService<IThemeService>();
            IEditorConfiguration configuration = services.GetRequiredService<IEditorConfiguration>();

            themeService.SetTheme(configuration.DefaultTheme);
            logger.LogInformation("Recursos del editor cargados, el tema inicial es {Theme}", configuration.DefaultTheme);

            MainWindowViewModel mainWindowViewModel = services.GetRequiredService<MainWindowViewModel>();
            MainWindow mainWindow = new MainWindow { DataContext = mainWindowViewModel };
            mainWindow.Closing += (_, _) => mainWindowViewModel.SaveWorkspaceLayout();

            desktop.MainWindow = mainWindow;
        }

        base.OnFrameworkInitializationCompleted();
    }
}