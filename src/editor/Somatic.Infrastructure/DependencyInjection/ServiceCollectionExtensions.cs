using Dock.Model.Core;
using Dock.Serializer.SystemTextJson;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Somatic.Core.Configuration;
using Somatic.Core.Localization;
using Somatic.Core.Themes;
using Somatic.Core.Viewport;
using Somatic.Core.Workspace;
using Somatic.Infrastructure.Configuration;
using Somatic.Infrastructure.Localization;
using Somatic.Infrastructure.Logging;
using Somatic.Infrastructure.Themes;
using Somatic.Infrastructure.Viewport;
using Somatic.Infrastructure.Workspace;

namespace Somatic.Infrastructure.DependencyInjection {
    public static class ServiceCollectionExtensions {
        public static IServiceCollection AddSomaticInfrastructure(this IServiceCollection services, IConfiguration configuration) {
            services.AddSingleton(configuration);
            services.AddOptions<EditorOptions>().Bind(configuration.GetSection(EditorOptions.SectionName));

            services.AddLogging(builder => builder.AddSerilog(SerilogLoggingBuilder.CreateLogger(), dispose: true));

            services.AddSingleton<IEditorConfiguration, EditorConfiguration>();
            services.AddSingleton<ILocalizationService, ResourceLocalizationService>();
            services.AddSingleton<IThemeService, ThemeService>();
            services.AddSingleton<IDockSerializer, DockSerializer>();
            services.AddSingleton<IWorkspaceLayoutService, FileWorkspaceLayoutService>();
            services.AddSingleton<IViewportSurfaceFactory, AnxietyEngineHost>();

            return services;
        }
    }
}
