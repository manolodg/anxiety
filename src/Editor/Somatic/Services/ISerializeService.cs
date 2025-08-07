namespace Somatic.Services {
    /// <summary>Realiza los servicios de serialización de los archivos.</summary>
    public interface ISerializeService {
#region Métodos
        /// <summary>Realiza la serialización al archivo JSON.</summary>
        /// <typeparam name="T">Tipo de datos a serializar.</typeparam>
        /// <param name="instance">Instancia a serializar.</param>
        /// <param name="path">Archivo sobre el que se serializa.</param>
        void ToFile<T>(T instance, string path);
        /// <summary>Realiza la deserialización de un archivo JSON.</summary>
        /// <typeparam name="T">Tipo de datos a deserializar.</typeparam>
        /// <param name="path">Archivo desde el que se deserializa.</param>
        /// <returns>Objeto deserializado.</returns>
        T FromFile<T>(string path);
#endregion
    }
}
