using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json;
using Somatic.Services;
using System.Collections.ObjectModel;
using System.Runtime.Serialization;

namespace Somatic.Model {
    /// <summary>Datos del proyecto, aquí se contiene toda la información para obtener tanto el Json como el compilado.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class Project : ObservableObject {
#region Constantes
        /// <summary>Extensión que utilizan los archivos de proyecto.</summary>
        public const string Extension = ".soma";
#endregion

#region Propiedades
        /// <summary>Nombre del proyecto.</summary>
        [JsonProperty] public string Name { get; set; } = null!;
        /// <summary>Descripción del proyecto.</summary>
        [JsonProperty] public string Description { get; set; } = null!;
        /// <summary>Raíz de la carpeta del proyecto.</summary>
        [JsonProperty] public string Path { get; set; } = null!;
        /// <summary>Ruta del archivo de escena por defecto.</summary>
        [JsonProperty] public string ActiveScenePath { get; set; } = null!;

        /// <summary>Ruta a los archivos de escena que incluye el proyecto.</summary>
        [JsonProperty] public ObservableCollection<string> ScenePaths { get; } = [];
        
        /// <summary>Motor de juego vinculado.</summary>
        [JsonProperty] public EngineTypes EngineType { get; set; }
        /// <summary>Modo del motor de juego.</summary>
        [JsonProperty] public EngineModes EngineMode { get; set; }

        /// <summary>Escena que está activa en el proyecto.</summary>
        public Scene ActiveScene { get; set; } = null!;
#endregion

#region Constructores
        public Project() {
            ActiveScene = new Scene { Name = "Scene", IsActive = true };
            ActiveScene.Entities.Add(new Entity { Name = "Entity", IsActive = true});
        }
        public Project(string name, string path, string description, EngineTypes type, EngineModes mode) {
            Name = name;
            Path = path;
            Description = description;
            EngineType = type;
            EngineMode = mode;

            ActiveScenePath = $"{Path}Assets{System.IO.Path.DirectorySeparatorChar}Scenes{System.IO.Path.DirectorySeparatorChar}{Framework.ServiceProvider!.GetRequiredService<ILocalizationService>().GetString("NewScene")}{Scene.Extension}";
            ScenePaths.Add(ActiveScenePath);

            ActiveScene = new Scene {
                Name = Framework.ServiceProvider!.GetRequiredService<ILocalizationService>().GetString("NewScene"),
                IsActive = true,
                Path = ActiveScenePath
            };
        }
#endregion

#region Métodos
    #region Serialización
        /// <summary>Realiza la serialización del archivo de escena activo.</summary>
        [OnSerialized]
        private void OnSerialized(StreamingContext context) {
            ISerializeService serialize = Framework.ServiceProvider.GetRequiredService<ISerializeService>();
            serialize.ToFile<Scene>(ActiveScene, ActiveScenePath);
        }
        /// <summary>Realiza la deserialización del archivo de escena por defecto.</summary>
        [OnDeserialized]
        private void OnDeserialized(StreamingContext context) {
            ISerializeService serialize = Framework.ServiceProvider.GetRequiredService<ISerializeService>();
            ActiveScene = serialize.FromFile<Scene>(ActiveScenePath);
        }
    #endregion
#endregion
    }
}
