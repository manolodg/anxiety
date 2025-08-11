using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Dock.Model.Controls;
using Dock.Model.Core;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Extensions;
using Somatic.ViewModels;
using Somatic.Views;

namespace Somatic {
    /// <summary>Punto de entrada de la aplicación Avalonia.</summary>
    public partial class App : Application {
        /// <summary>Inicia toda la interfaz gráfica.</summary>
        public override void Initialize() => AvaloniaXamlLoader.Load(this);

        /// <summary>Inicia los servicios y pantalla de la aplicación.</summary>
        public override void OnFrameworkInitializationCompleted() {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) {
                // Ser realiza el arranque y configuración de la inyección de dependencias.
                ServiceProvider services = new ServiceCollection().ConfigureServices();

                IRootDock layout = services.GetRequiredService<IRootDock>();

                MainWindow mainWindow = services.GetRequiredService<MainWindow>();
                mainWindow.DataContext = services.GetRequiredService<MainWindowViewModel>();
                mainWindow.Show();
                mainWindow.Focus();

                mainWindow.Closing += (_, _) => { if (layout is IDock dock && dock.Close.CanExecute(null)) dock.Close.Execute(null); };

                desktop.MainWindow = mainWindow;

                desktop.Exit += (_, _) => { if (layout is IDock dock && dock.Close.CanExecute(null)) dock.Close.Execute(null); };
            }

            base.OnFrameworkInitializationCompleted();
        }
    }
}