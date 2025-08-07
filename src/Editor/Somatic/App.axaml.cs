using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Extensions;
using Somatic.Views;

namespace Somatic {
    /// <summary>Punto de entrada de la aplicaci�n Avalonia.</summary>
    public partial class App : Application {
        /// <summary>Inicia toda la interfaz gr�fica.</summary>
        public override void Initialize() => AvaloniaXamlLoader.Load(this);

        /// <summary>Inicia los servicios y pantalla de la aplicaci�n.</summary>
        public override void OnFrameworkInitializationCompleted() {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) {
                // Ser realiza el arranque y configuraci�n de la inyecci�n de dependencias.
                ServiceProvider services = new ServiceCollection().ConfigureServices();

                desktop.MainWindow = new MainWindow();
            }

            base.OnFrameworkInitializationCompleted();
        }
    }
}