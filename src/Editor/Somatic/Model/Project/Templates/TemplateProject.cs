using Avalonia.Media.Imaging;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Somatic.Model {
    /// <summary>Plantilla de un proyecto.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public class TemplateProject {
#region Propiedades
        /// <summary>Nombre de la plantilla.</summary>
        [JsonProperty] public string Name { get; set; } = null!;
        /// <summary>Descripción de la plantilla.</summary>
        [JsonProperty] public string Description { get; set; } = null!;
        /// <summary>Carpetas que ha de crear la plantilla.</summary>
        [JsonProperty] public string[] Folders { get; set; } = null!;
        /// <summary>Tipo de motor de juego.</summary>
        [JsonConverter(typeof(StringEnumConverter))] public EngineTypes EngineType { get; set; }
        /// <summary>Modo dentro del tipo de motor.</summary>
        [JsonConverter(typeof(StringEnumConverter))] public EngineModes EngineMode { get; set; }

        /// <summary>Icono de la plantilla.</summary>
        public Bitmap Icon { get; set; } = null!;
        /// <summary>Previo de la plantilla.</summary>
        public Bitmap Screenshot { get; set; } = null!;

        /// <summary>Ruta del icono.</summary>
        public string IconPath { get; set; } = null!;
        /// <summary>Ruta de la preview.</summary>
        public string ScreenshotPath { get; set; } = null!;
        /// <summary>Ruta de la plantilla.</summary>
        public string TemplateProjectPath { get; set; } = null!;
#endregion
    }
}
