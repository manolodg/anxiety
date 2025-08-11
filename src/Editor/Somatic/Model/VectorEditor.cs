using Newtonsoft.Json;

namespace Somatic.Model {
    /// <summary>Valor que se utiliza para editar un vector.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public class VectorEditor {
#region Propiedades
        /// <summary>Propiedad de la coordenada X.</summary>
        [JsonProperty] public float X { get; set; }
        /// <summary>Propiedad de la coordenada Y.</summary>
        [JsonProperty] public float Y { get; set; }
        /// <summary>Propiedad de la coordenada Z.</summary>
        [JsonProperty] public float Z { get; set; }
#endregion

#region Constructor
        /// <summary>Crea una instancia de la clase <see cref="VectorEditor"/>.</summary>
        public VectorEditor(float x, float y, float z) {
            X = x; 
            Y = y; 
            Z = z;
        }
#endregion
    }
}
