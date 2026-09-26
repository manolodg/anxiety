using CommunityToolkit.Mvvm.ComponentModel;
using Somatic.Core.History;
using Somatic.Core.Scene.Components;
using System.Collections.ObjectModel;

namespace Somatic.Core.Scene {
    /// <summary>
    /// Nodo del grafo de escena. Siempre tiene una <see cref="TransformComponent"/> (primera de la lista, no
    /// se puede quitar). Los hijos se gestionan a través de <see cref="SceneGraph"/>, que mantiene la
    /// coherencia entre entidades raíz y anidadas.
    /// </summary>
    public sealed partial class Entity : ObservableObject {
        private readonly ObservableCollection<Entity>    _children   = [];
        private readonly ObservableCollection<Component> _components = [];

        [ObservableProperty] private string _name;
        [ObservableProperty] private bool _isActive = true;
        /// <summary>Estado de despliegue en la jerarquía del editor.</summary>
        [ObservableProperty] [property: UndoIgnore] private bool _isExpanded;

        public Guid Id { get; } = Guid.NewGuid();
        public EntityKind Kind { get; }
        public Entity? Parent { get; private set; }
        public TransformComponent Transform { get; }

        public ReadOnlyObservableCollection<Entity>    Children   { get; }
        public ReadOnlyObservableCollection<Component> Components { get; }

        public Entity(EntityKind kind, string name) {
            Kind = kind;
            _name = name;
            Children   = new ReadOnlyObservableCollection<Entity>(_children);
            Components = new ReadOnlyObservableCollection<Component>(_components);
            Transform  = AddComponent(new TransformComponent());
        }

        public T AddComponent<T>(T component) where T : Component => AddComponent(component, -1);

        /// <summary>Añade el componente en <paramref name="index"/> (negativo = al final). Lo usa deshacer para devolverlo a su sitio.</summary>
        public T AddComponent<T>(T component, int index) where T : Component {
            if (component.Owner is not null) throw new InvalidOperationException("El componente ya pertenece a otra entidad.");
            if (HasComponent(component.GetType())) throw new InvalidOperationException($"La entidad '{Name}' ya tiene un componente {component.GetType().Name}.");

            component.Owner = this;
            _components.Insert(index < 0 || index > _components.Count ? _components.Count : index, component);
            return component;
        }

        public bool RemoveComponent(Component component) {
            if (!component.CanRemove || !_components.Remove(component)) return false;

            component.Owner = null;
            return true;
        }

        public int IndexOfComponent(Component component) => _components.IndexOf(component);

        public T? GetComponent<T>() where T : Component => _components.OfType<T>().FirstOrDefault();

        public bool HasComponent(Type componentType) => _components.Any(c => c.GetType() == componentType);

        /// <summary><c>true</c> si <paramref name="entity"/> cuelga (a cualquier profundidad) de esta entidad.</summary>
        public bool IsAncestorOf(Entity entity) {
            for (Entity? current = entity.Parent; current is not null; current = current.Parent) {
                if (current == this) return true;
            }
            return false;
        }

        internal void InsertChild(Entity child, int index) {
            child.Parent = this;
            _children.Insert(index < 0 || index > _children.Count ? _children.Count : index, child);
        }

        internal bool RemoveChild(Entity child) {
            if (!_children.Remove(child)) return false;

            child.Parent = null;
            return true;
        }

        public override string ToString() => Name;
    }
}
