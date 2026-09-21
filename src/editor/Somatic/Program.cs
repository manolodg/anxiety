using Avalonia;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Serilog;
using Somatic.Infrastructure.DependencyInjection;
using Somatic.ViewModels;
using Somatic.ViewModels.Docking;

namespace Somatic {
    internal class Program {
        [STAThread]
        public static int Main(string[] args) {
            IConfigurationRoot configuration = new ConfigurationBuilder()
                .SetBasePath(AppContext.BaseDirectory)
                .AddJsonFile("appsettings.json", optional: false, reloadOnChange: false)
                .Build();

            ServiceCollection services = new ServiceCollection();
            services.AddSomaticInfrastructure(configuration);
            services.AddSingleton<MainLayoutFactory>();
            services.AddSingleton<MainWindowViewModel>();

            App.Services = services.BuildServiceProvider();

            ILogger<App> logger = App.Services.GetRequiredService<ILogger<App>>();
            AppDomain.CurrentDomain.UnhandledException += (_, e) => logger.LogCritical(e.ExceptionObject as Exception, "Excepción no gestionada");

            try {
                logger.LogInformation("Arrancando el editor Somatic");
                return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
            } finally {
                // Libera los singletons IDisposable (p. ej. el motor nativo) antes de cerrar el log.
                (App.Services as IDisposable)?.Dispose();
                Log.CloseAndFlush();
            }
        }

        public static AppBuilder BuildAvaloniaApp() =>
            AppBuilder.Configure<App>()
                .UsePlatformDetect()
                .LogToTrace();
    }
}
