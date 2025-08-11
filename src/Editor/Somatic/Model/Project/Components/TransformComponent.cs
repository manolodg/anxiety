using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json;
using Somatic.Services;

namespace Somatic.Model {
    /// <summary>Componente del tipo transformación.</summary>
    [JsonObject(MemberSerialization.OptIn)]
    public partial class TransformComponent : BaseComponent {
#region Propiedades Avalonia
        /// <summary>Vector de posición.</summary>
        [JsonProperty][ObservableProperty] private VectorEditor _position = new VectorEditor(0, 0, 0);
        /// <summary>Vector de rotación.</summary>
        [JsonProperty][ObservableProperty] private VectorEditor _rotation = new VectorEditor(0, 0, 0);
        /// <summary>Vector de escala.</summary>
        [JsonProperty][ObservableProperty] private VectorEditor _scale = new VectorEditor(1, 1, 1);
#endregion

#region Propiedades
#pragma warning disable MVVMTK0034 // Direct field reference to [ObservableProperty] backing field
        /// <summary>X del vector de posición.</summary>
        public float PositionX {
            get => Position.X;
            set { if (value != Position.X) _position.X = value; }
        }
        /// <summary>Y del vector de posición.</summary>
        public float PositionY {
            get => Position.Y;
            set { if (value != Position.Y) _position.Y = value; }
        }
        /// <summary>Z del vector de posición.</summary>
        public float PositionZ {
            get => Position.Z;
            set { if (value != Position.Z) _position.Z = value; }
        }

        /// <summary>X del vector de rotación.</summary>
        public float RotationX {
            get => Rotation.X;
            set { if (value != Rotation.X) _rotation.X = value; }
        }
        /// <summary>Y del vector de rotación.</summary>
        public float RotationY {
            get => Rotation.Y;
            set { if (value != Rotation.Y) _rotation.Y = value; }
        }
        /// <summary>Z del vector de rotación.</summary>
        public float RotationZ {
            get => Rotation.Z;
            set { if (value != Rotation.Z) _rotation.Z = value; }
        }

        /// <summary>X del vector de escala.</summary>
        public float ScaleX {
            get => Scale.X;
            set { if (value != Scale.X) _scale.X = value; }
        }
        /// <summary>Y del vector de escala.</summary>
        public float ScaleY {
            get => Scale.Y;
            set { if (value != Scale.Y) _scale.Y = value; }
        }
        /// <summary>Z del vector de escala.</summary>
        public float ScaleZ {
            get => Scale.Z;
            set { if (value != Scale.Z) _scale.Z = value; }
        }
#pragma warning restore MVVMTK0034 // Direct field reference to [ObservableProperty] backing field
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase <see cref="TransformComponent"/>.</summary>
        public TransformComponent() {
            Name = Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("Transform_title") ?? "Transform Component";
        }
#endregion
    }
}
