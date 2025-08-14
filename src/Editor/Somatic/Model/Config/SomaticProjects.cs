using Newtonsoft.Json;
using System.Collections.Generic;

namespace Somatic.Model {
    /// <summary>Historico de proyectos.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public class SomaticProjects {
#region Propiedades
        /// <summary>Lista de proyectos.</summary>
        [JsonProperty] public List<ProjectData> Projects { get; set; } = [];
        /// <summary>Rutas de los proyectos.</summary>
        [JsonProperty] public List<string> Paths { get; set; } = [];
#endregion
    }
}
