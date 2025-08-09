using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Somatic.Extensions;
using Somatic.Services;
using System;
using System.IO;

namespace Somatic {
    /// <summary>Clase que se utiliza como CrossLayer y configuración de la aplicación.</summary>
    public static class Framework {
#region Propiedades
        /// <summary>Exposición del proveedor de inyección de dependencias.</summary>
        public static ServiceProvider ServiceProvider { get; set; } = null!;
#endregion

#region Métodos
        /// <summary>Configura el sistema de logging de la aplicación.</summary>
        public static void ConfigureLogger() {
            string logDirectory = Path.Combine(AppContext.BaseDirectory, "Logs");
            if (!Directory.Exists(logDirectory)) Directory.CreateDirectory(logDirectory);

            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Debug()
                .MinimumLevel.Override("Microsoft", Serilog.Events.LogEventLevel.Information)
                .Enrich.FromLogContext()
                .WriteTo.Avalonia()
                .WriteTo.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:u3}] {Message:lj}{NewLine}{Exception}")
                .WriteTo.File(
                        Path.Combine(logDirectory, "log-.txt"),
                        rollingInterval: RollingInterval.Day,
                        retainedFileCountLimit: 3,
                        fileSizeLimitBytes: 10 * 1024 * 1024,
                        outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}",
                        rollOnFileSizeLimit: true)
                .CreateLogger();
        }

        /// <summary>Configuración de los elementos que se introducen en la inyección de dependencias.</summary>
        /// <param name="services">Colección de servicios a la que se vinculan.</param>
        public static void ConfigureServices(ServiceCollection services) {
            // Configuración ----------------------------------------------------------------------
            services.AddLogging(loggingBuilder => loggingBuilder.AddSerilog(dispose: true));
            // ---------------------------------------------------------------------- Configuración

            // Servicios --------------------------------------------------------------------------
            services.AddSingleton<ISerializeService, SerializeService>();
            services.AddSingleton<ILocalizationService, LocalizationService>();
            // -------------------------------------------------------------------------- Servicios
        }
#endregion
    }
}
