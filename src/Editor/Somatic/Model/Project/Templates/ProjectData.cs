using Avalonia.Media.Imaging;
using Newtonsoft.Json;
using System;

namespace Somatic.Model {
    /// <summary>Datos del proyecto.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public class ProjectData {
#region Propiedades
        /// <summary>Nombre del proyecto.</summary>
        [JsonProperty] public string Name { get; set; } = null!;
        /// <summary>Ruta del proyecto.</summary>
        [JsonProperty] public string Path { get; set; } = null!;
        /// <summary>Descripción del proyecto.</summary>
        [JsonProperty] public string Description { get; set; } = null!;
        /// <summary>Última vez que se ha actualizado el proyecto.</summary>
        [JsonProperty] public DateTime LastTime { get; set; }

        /// <summary>Icono vinculado al proyecto.</summary>
        public Bitmap Icon { get; set; } = null!;
        /// <summary>Imagen previa del proyecto.</summary>
        public Bitmap Screenshot { get; set; } = null!;
#endregion
    }
}
