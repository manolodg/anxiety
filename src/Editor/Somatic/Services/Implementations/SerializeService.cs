using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using System;
using System.IO;

namespace Somatic.Services {
    /// <summary>Realiza los servicios de serialización de los archivos.</summary>
    public class SerializeService(ILogger<SerializeService> logger) : ISerializeService {
#region Campos
        // Configuración del sistema de serialización/deserialización.
        private JsonSerializerSettings _settings = new JsonSerializerSettings {
            Formatting = Formatting.Indented,
            NullValueHandling = NullValueHandling.Ignore,
            TypeNameHandling = TypeNameHandling.Auto,
            DateFormatString = "yyyy-MM-dd HH:mm:ss",
            PreserveReferencesHandling = PreserveReferencesHandling.Objects
        };
#endregion

#region Métodos
        /// <summary>Realiza la serialización al archivo JSON.</summary>
        /// <typeparam name="T">Tipo de datos a serializar.</typeparam>
        /// <param name="instance">Instancia a serializar.</param>
        /// <param name="path">Archivo sobre el que se serializa.</param>
        public void ToFile<T>(T instance, string path) {
            try {
                string json = JsonConvert.SerializeObject(instance, _settings);
                File.WriteAllText(path, json);
            } catch (Exception ex) {
                logger.LogError(ex, "No se ha podido serializar el archivo");
            }
        }

        /// <summary>Realiza la deserialización de un archivo JSON.</summary>
        /// <typeparam name="T">Tipo de datos a deserializar.</typeparam>
        /// <param name="path">Archivo desde el que se deserializa.</param>
        /// <returns>Objeto deserializado.</returns>
        public T FromFile<T>(string path) {
            try {
                string serie = File.ReadAllText(path);
                return JsonConvert.DeserializeObject<T>(serie, _settings)!;
            } catch (Exception ex) {
                logger.LogError(ex, "No se ha podido deserializar el archivo");

                return default!;
            }
        }
#endregion
    }
}
