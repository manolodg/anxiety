using Serilog;
using Serilog.Configuration;
using Serilog.Core;
using Serilog.Events;
using Somatic.Model;

namespace Somatic.Extensions {
    /// <summary>Extensión de Avalonia para capturar el Sink y utilizar la pantalla de Log en Avalonia.</summary>
    public static class LoggerConfigurationExtension {
#region Métodos
        /// <summary>Configuración del Sink enlazado en Avalonia.</summary>
        public static LoggerConfiguration Avalonia(this LoggerSinkConfiguration config, LogEventLevel minimumLevel = LevelAlias.Minimum, LoggingLevelSwitch? levelSwitch = null) {
            return config.Sink(new AvaloniaSink(), minimumLevel, levelSwitch);
        }
#endregion
    }
}
