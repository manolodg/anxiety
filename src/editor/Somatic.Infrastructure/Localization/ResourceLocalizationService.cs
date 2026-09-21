using Microsoft.Extensions.Logging;
using Somatic.Core.Configuration;
using Somatic.Core.Localization;
using System.Globalization;
using System.Resources;

namespace Somatic.Infrastructure.Localization {
    public sealed class ResourceLocalizationService : ILocalizationService {
        public CultureInfo CurrentCulture { get; private set; }

        private static readonly ResourceManager ResourceManager = new("Somatic.Infrastructure.Localization.Resources.Strings", typeof(ResourceLocalizationService).Assembly);

        private readonly ILogger<ResourceLocalizationService> _logger;

        public ResourceLocalizationService(ILogger<ResourceLocalizationService> logger, IEditorConfiguration configuration) {
            _logger = logger;
            CurrentCulture = TryCreateCulture(configuration.Language) ?? CultureInfo.GetCultureInfo("en");
        }

        public string GetString(string key) {
            string? value = ResourceManager.GetString(key, CurrentCulture);
            if (value is not null)  return value;

            _logger.LogWarning("Clave de localización '{Key}' no encontrada para el idioma '{Culture}'", key, CurrentCulture.Name);
            return key;
        }

        public void SetCulture(CultureInfo culture) {
            CurrentCulture = culture;
            _logger.LogInformation("Idioma activado a {Culture}", culture.Name);
        }

        private static CultureInfo? TryCreateCulture(string name) {
            try {
                return CultureInfo.GetCultureInfo(name);
            } catch (CultureNotFoundException) {
                return null;
            }
        }
    }
}
