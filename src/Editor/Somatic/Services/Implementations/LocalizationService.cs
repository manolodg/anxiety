using System.Globalization;
using System.Resources;
using System.Threading;

namespace Somatic.Services {
    /// <summary>Servicio de localización.</summary>
    public class LocalizationService : ILocalizationService {
#region Propiedades
        /// <summary>Idioma actual del servicio.</summary>
        public string CurrentLanguage => _currentCulture.Name;
#endregion

#region Campos
        // Administrador de recursos para la gestión de los archivos de recursos.
        private readonly ResourceManager _resourceManager;

        // Cultura actual del servicio.
        private CultureInfo _currentCulture;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase <see cref="LocalizationService"/>.</summary>
        public LocalizationService() {
            _resourceManager = new ResourceManager("Somatic.Assets.Languages.Strings", typeof(LocalizationService).Assembly);
            _currentCulture = CultureInfo.CurrentCulture;
        }
#endregion

#region Métodos
        /// <summary>Obtiene la traducción vinculada a la clave indicada por el usuario.</summary>
        public string GetString(string key) => _resourceManager.GetString(key, _currentCulture) ?? key;
        /// <summary>Establece el idioma actual del sistema.</summary>
        /// <param name="languageCode">Código regional del lenguaje que queremos activar.</param>
        public void SetLanguage(string languageCode) {
            _currentCulture = CultureInfo.GetCultureInfo(languageCode);

            Thread.CurrentThread.CurrentCulture = _currentCulture;
            Thread.CurrentThread.CurrentUICulture = _currentCulture;
        }
#endregion
    }
}