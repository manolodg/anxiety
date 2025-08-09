namespace Somatic.Services {
    /// <summary>Servicio de localización para su gestión dentro del código.</summary>
    public interface ILocalizationService {
#region Propiedades
        /// <summary>Idioma actual del servicio.</summary>
        string CurrentLanguage { get; }
#endregion

#region Métodos
        /// <summary>Obtiene la traducción vinculada a la clave indicada por el usuario.</summary>
        /// <param name="key">Clave indicada por el usuario.</param>
        /// <returns>Si la clave tiene traducción devuelve la traducción, si no devuelve la misma clave.</returns>
        string GetString(string key);
        /// <summary>Establece el idioma actual del sistema.</summary>
        /// <param name="languageCode">Código regional del lenguaje que queremos activar.</param>
        void SetLanguage(string languageCode);
#endregion
    }
}