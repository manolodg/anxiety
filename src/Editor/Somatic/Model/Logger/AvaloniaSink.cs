using Microsoft.Extensions.DependencyInjection;
using Serilog.Core;
using Serilog.Events;
using Somatic.ViewModels;

namespace Somatic.Model {
    /// <summary>Punto de escritura de los sistemas de logging.</summary>
    public class AvaloniaSink : ILogEventSink {
#region Campos
        /// <summary>Modelo del logger que utilizamos.</summary>
        private static LoggerViewModel _viewModel = null!;
#endregion

#region Métodos
        /// <summary>Realiza el guardado del log en el sistema.</summary>
        /// <param name="logEvent">Evento de log a procesar.</param>
        public void Emit(LogEvent logEvent) {
            if (_viewModel == null && Framework.ServiceProvider != null) {
                _viewModel = Framework.ServiceProvider.GetRequiredService<LoggerViewModel>();
                _viewModel?.AddMessage(new LogMessage(logEvent));
            } else {
                _viewModel?.AddMessage(new LogMessage(logEvent));
            }
        }
#endregion
    }
}
