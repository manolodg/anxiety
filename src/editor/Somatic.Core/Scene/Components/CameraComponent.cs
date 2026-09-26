using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.Core.Scene.Components {
    public enum CameraProjection {
        Perspective,
        Orthographic
    }

    public sealed partial class CameraComponent : Component {
        [ObservableProperty] private CameraProjection _projection = CameraProjection.Perspective;
        [ObservableProperty] private float _fieldOfView     = 60.0f;
        [ObservableProperty] private float _orthographicSize = 5.0f;
        [ObservableProperty] private float _nearPlane       = 0.1f;
        [ObservableProperty] private float _farPlane        = 1000.0f;
    }
}
