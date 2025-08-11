using Newtonsoft.Json;

namespace Somatic.Model {
    /// <summary>Entidad común que incluye por defecto la transformación.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class Entity : BaseEntity {
#region Constructor
        /// <summary>Añade a la entidad el componente de transformación.</summary>
        public Entity() {
            Components.Add(new TransformComponent());
        }
#endregion
    }
}
