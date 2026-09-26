using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.Core.Scene.Components {
    /// <summary>
    /// Base de todos los componentes de una entidad. Cada componente concreto añade sus propiedades
    /// observables; el inspector elige el editor adecuado según el tipo concreto.
    /// </summary>
    public abstract partial class Component : ObservableObject {
        [ObservableProperty] private bool _isEnabled = true;

        /// <summary>Entidad propietaria; la asigna <see cref="Entity"/> al añadir o quitar el componente.</summary>
        public Entity? Owner { get; internal set; }

        /// <summary>Si es <c>false</c> el componente es parte fija de la entidad (p. ej. la transformación).</summary>
        public virtual bool CanRemove => true;

        /// <summary>Si es <c>false</c> el componente no puede desactivarse desde el inspector.</summary>
        public virtual bool CanDisable => true;
    }
}
