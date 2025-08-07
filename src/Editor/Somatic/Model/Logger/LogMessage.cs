using Serilog.Events;
using System;

namespace Somatic.Model {
    /// <summary>Mensaje que se muestra en el log.</summary>
    public class LogMessage {
#region Propiedades
        /// <summary>Fecha y hora del log</summary>
        public DateTime Time { get; set; }
        /// <summary>Tipo de log</summary>
        public MessageType MessageType { get; set; }
        /// <summary>Mensaje que se pone en el log</summary>
        public string Message { get; set; }
#endregion

#region Constructores
        /// <summary>Construye y configura el log que se produce.</summary>
        public LogMessage(LogEvent log) {
            Time = log.Timestamp.DateTime;
            MessageType = log.Level switch {
                LogEventLevel.Verbose or LogEventLevel.Information or LogEventLevel.Debug   => MessageType.Information,
                LogEventLevel.Warning                                                       => MessageType.Warning,
                LogEventLevel.Error or LogEventLevel.Fatal                                  => MessageType.Error,
                _                                                                           => MessageType.Information
            };
            Message = log.RenderMessage();
        }
#endregion
    }
}
