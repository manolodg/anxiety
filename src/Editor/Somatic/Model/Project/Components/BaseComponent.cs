using CommunityToolkit.Mvvm.ComponentModel;
using Newtonsoft.Json;

namespace Somatic.Model {
    /// <summary>Componente base en el que todos los componentes se basan</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public abstract partial class BaseComponent : ObservableObject {
#region Propiedades
        /// <summary>Nombe del componente.</summary>
        [JsonProperty] [ObservableProperty] private string _name = null!;
        /// <summary>Indica si el componente está o no activo.</summary>
        [JsonProperty] [ObservableProperty] private bool _isActive = true;
        /// <summary>Entidad propietaria de este componente.</summary>
        [JsonProperty] [ObservableProperty] private BaseEntity _owner = null!;
        /// <summary>Ordenación dentro de la entidad de los componentes.</summary>
        [JsonProperty] [ObservableProperty] private int _order = 0;
#endregion
    }
}
