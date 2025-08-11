using System;
using Avalonia;
using Serilog;

namespace Somatic {
    /// <summary>Punto de entrada de la aplicación.</summary>
    internal class Program {
        /// <summary>
        /// Código de inicialización. No utilices Avalinia, APIs de terceros o cualquier código de Sincronización de
        /// contexto antes de llamar AppMain: Las cosas no estan iniciadas aún y se puede romper.
        /// </summary>
        /// <param name="args">Argumentos introducidos en la línea de comandos.</param>
        [STAThread]
        public static void Main(string[] args) {
            try {
                // Configuración de Serilog...
                Framework.ConfigureLogger();
                Log.Information("Iniciando la aplicación...");

                BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

                Log.Information("Saliendo de la aplicación de forma correcta.");
            } catch (Exception ex) {
                Log.Fatal(ex, "La aplicación ha acabado de forma incorrecta.");
            } finally {
                Log.CloseAndFlush();
            }
        }

        /// <summary>Configuración de Avalonia, no borrar...</summary>
        public static AppBuilder BuildAvaloniaApp() => AppBuilder
            .Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
    }
}
