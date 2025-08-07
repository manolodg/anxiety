using Serilog.Core;
using Serilog.Events;

namespace Somatic.Model {
    public class AvaloniaSink : ILogEventSink {
        // private static LoggerViewModel _viewModel = null!;
        // 
        // public void Emit(LogEvent logEvent) {
        //     if (_viewModel == null && Framework.ServiceProvider != null) {
        //         _viewModel = Framework.ServiceProvider.GetRequiredService<LoggerViewModel>();
        //         _viewModel?.AddMessage(new LogMessage(logEvent));
        //     } else {
        //         _viewModel?.AddMessage(new LogMessage(logEvent));
        //     }
        // }

        public void Emit(LogEvent logEvent) { }
    }
}
