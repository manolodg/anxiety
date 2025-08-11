using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json;
using Somatic.Services;
using System.Collections.ObjectModel;

namespace Somatic.Model {
    /// <summary>Componente del tipo script.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class ScriptComponent : BaseComponent {
#region Propiedades Avalonia
        [JsonProperty] [ObservableProperty] private string _scriptName = null!;
        [JsonProperty] [ObservableProperty] private string _path = null!;
        [JsonProperty] [ObservableProperty] private ObservableCollection<ScriptParameter> _parameters = [];
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase <see cref="TransformComponent"/>.</summary>
        public ScriptComponent() {
            Name = Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("Script_title") ?? "Script Component";
        }
#endregion
    }
}
