using Newtonsoft.Json;

namespace Somatic.Model {
    [JsonObject(MemberSerialization.OptIn)]
    public partial class Entity : BaseEntity {
    }
}
