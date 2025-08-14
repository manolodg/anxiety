using CommunityToolkit.Mvvm.ComponentModel;
using Newtonsoft.Json;
using System.Collections.ObjectModel;

namespace Somatic.Model {
    /// <summary>Definición base de las entidades.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public abstract partial class BaseEntity : ObservableObject {
#region Propiedades
        /// <summary>Nombre de la entidad.</summary>
        [JsonProperty] [ObservableProperty] private string _name = null!;
        /// <summary>Indica si la entidad esta o no activa.</summary>
        [JsonProperty] [ObservableProperty] private bool _isActive;

        /// <summary>Componentes que contiene esta entidad.</summary>
        [JsonProperty] public ObservableCollection<BaseComponent> Components { get; } = [];
        /// <summary>Entidades hijas.</summary>
        [JsonProperty] public ObservableCollection<BaseEntity> Children { get; } = [];
#endregion
    }
}
