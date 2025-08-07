using CommunityToolkit.Mvvm.ComponentModel;
using Newtonsoft.Json;
using System.Collections.ObjectModel;

namespace Somatic.Model {
    /// <summary>Definición de una escena dentro del proyecto.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class Scene : ObservableObject {
#region Constantes
        /// <summary>Extensión que utilizan los archivos de escena.</summary>
        public const string Extension = ".scene";
#endregion

#region Propiedades
        /// <summary>Nombre de la escena.</summary>
        [JsonProperty] public string Name { get; set; } = null!;
        /// <summary>Ruta del archivo de la escena.</summary>
        [JsonProperty] public string Path { get; set; } = null!;
        /// <summary>Indica si la escena es la activa o no.</summary>
        [JsonProperty] public bool IsActive { get; set; }
        /// <summary>Todas las entidades que tiene la escena a nivel raíz.</summary>
        [JsonProperty] public ObservableCollection<BaseEntity> Entities { get; } = [];
#endregion
    }
}
