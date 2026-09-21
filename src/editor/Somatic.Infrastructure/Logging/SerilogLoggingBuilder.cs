using Serilog;
using Serilog.Events;

namespace Somatic.Infrastructure.Logging {
    public static class SerilogLoggingBuilder {
        public static Serilog.ILogger CreateLogger() =>
            new LoggerConfiguration()
                .MinimumLevel.Information()
                .MinimumLevel.Override("Microsoft", LogEventLevel.Warning)
                .Enrich.FromLogContext()
                .WriteTo.Console()
                .WriteTo.Debug()
                .CreateLogger();
    }
}
