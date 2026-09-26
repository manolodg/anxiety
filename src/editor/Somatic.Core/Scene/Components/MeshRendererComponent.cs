using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.Core.Scene.Components {
    public sealed partial class MeshRendererComponent : Component {
        /// <summary>Ruta del recurso de malla dentro del proyecto.</summary>
        [ObservableProperty] private string _mesh     = string.Empty;
        /// <summary>Ruta del recurso de material dentro del proyecto.</summary>
        [ObservableProperty] private string _material = string.Empty;
        [ObservableProperty] private bool _castShadows    = true;
        [ObservableProperty] private bool _receiveShadows = true;
    }
}
