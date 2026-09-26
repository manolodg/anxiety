using CommunityToolkit.Mvvm.ComponentModel;
using System.Numerics;

namespace Somatic.Core.Scene.Components {
    /// <summary>Posición, rotación (ángulos de Euler en grados) y escala locales respecto al padre. Toda entidad tiene una.</summary>
    public sealed partial class TransformComponent : Component {
        [ObservableProperty] private Vector3 _position = Vector3.Zero;
        [ObservableProperty] private Vector3 _rotation = Vector3.Zero;
        [ObservableProperty] private Vector3 _scale    = Vector3.One;

        public override bool CanRemove  => false;
        public override bool CanDisable => false;
    }
}
