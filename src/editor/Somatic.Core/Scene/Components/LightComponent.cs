using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.Core.Scene.Components {
    public enum LightType {
        Directional,
        Point,
        Spot
    }

    public sealed partial class LightComponent : Component {
        [ObservableProperty] private LightType _type = LightType.Point;
        [ObservableProperty] private ColorRgba _color = ColorRgba.White;
        [ObservableProperty] private float _intensity = 1.0f;
        [ObservableProperty] private float _range     = 10.0f;
        [ObservableProperty] private float _spotAngle = 30.0f;
        [ObservableProperty] private bool  _castShadows = true;
    }
}
