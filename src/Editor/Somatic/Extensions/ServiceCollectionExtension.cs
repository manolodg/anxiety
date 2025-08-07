using Microsoft.Extensions.DependencyInjection;

namespace Somatic.Extensions {
    /// <summary>Extensión al <see cref="ServiceCollection"/> que se utiliza para la inyección de dependencias.</summary>
    public static class ServiceCollectionExtension {
        /// <summary>Configuración de todos los servicios que se inyectan para las dependencias.</summary>
        /// <param name="services">Punto de extensión de los servicios.</param>
        /// <returns>Proveedor de servicios para extracción de las dependencias.</returns>
        public static ServiceProvider ConfigureServices(this ServiceCollection services) {
            Framework.ConfigureServices(services);
            Framework.ServiceProvider = services.BuildServiceProvider();

            return Framework.ServiceProvider;
        }
    }
}
