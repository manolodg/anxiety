using CommunityToolkit.Mvvm.ComponentModel;
using Newtonsoft.Json;
using System;

namespace Somatic.Model {
    /// <summary>Parámetros aceptados por un Script.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class ScriptParameter : ObservableObject {
#region Propiedades Avalonia
        [JsonProperty][ObservableProperty] private Type _type = null!;
        [JsonProperty][ObservableProperty] private string _name = null!;
        [JsonProperty][ObservableProperty] private object _value = null!;
#endregion
    }
}
