using Somatic.Core.History;
using Somatic.Core.Scene;
using Somatic.Core.Scene.Components;
using System.Reflection;

namespace Somatic.Infrastructure.Scene {
    // Comandos del historial para las operaciones de estructura de la escena. Todos dejan seleccionada la
    // entidad afectada, para que al deshacer/rehacer se vea en el inspector qué ha cambiado.

    internal sealed class CreateEntityCommand(SceneService scene, Entity entity, Entity? parent, string description) : IUndoableCommand {
        private readonly Entity? _previousSelection = scene.SelectedEntity;

        public string Description => description;

        public void Execute() {
            scene.ActiveScene.Add(entity, parent);
            if (parent is not null) parent.IsExpanded = true;
            scene.SelectedEntity = entity;
        }

        public void Undo() {
            scene.ActiveScene.Remove(entity);
            scene.SelectedEntity = _previousSelection;
        }
    }

    internal sealed class DeleteEntityCommand(SceneService scene, Entity entity, string description) : IUndoableCommand {
        private Entity? _parent;
        private int     _index;
        private Entity? _previousSelection;

        public string Description => description;

        public void Execute() {
            _parent = entity.Parent;
            _index  = scene.ActiveScene.IndexOf(entity);
            _previousSelection = scene.SelectedEntity;

            bool selectionInside = _previousSelection is not null && (_previousSelection == entity || entity.IsAncestorOf(_previousSelection));
            scene.ActiveScene.Remove(entity);
            if (selectionInside) scene.SelectedEntity = _parent;
        }

        public void Undo() {
            scene.ActiveScene.Add(entity, _parent, _index);
            scene.SelectedEntity = _previousSelection;
        }
    }

    internal sealed class MoveEntityCommand(SceneService scene, Entity entity, Entity? oldParent, int oldIndex, Entity? newParent, int newIndex, string description) : IUndoableCommand {
        public string Description => description;

        public void Execute() => MoveTo(newParent, newIndex);

        public void Undo() => MoveTo(oldParent, oldIndex);

        private void MoveTo(Entity? parent, int index) {
            scene.ActiveScene.MoveTo(entity, parent, index);
            if (parent is not null) parent.IsExpanded = true;
            scene.SelectedEntity = entity;
        }
    }

    internal sealed class AddComponentCommand(SceneService scene, Entity entity, Component component, string description) : IUndoableCommand {
        private int _index = -1;

        public string Description => description;

        public void Execute() {
            entity.AddComponent(component, _index);
            _index = entity.IndexOfComponent(component);
            scene.SelectedEntity = entity;
        }

        public void Undo() {
            entity.RemoveComponent(component);
            scene.SelectedEntity = entity;
        }
    }

    internal sealed class RemoveComponentCommand(SceneService scene, Entity entity, Component component, string description) : IUndoableCommand {
        private int _index;

        public string Description => description;

        public void Execute() {
            _index = entity.IndexOfComponent(component);
            entity.RemoveComponent(component);
            scene.SelectedEntity = entity;
        }

        public void Undo() {
            entity.AddComponent(component, _index);
            scene.SelectedEntity = entity;
        }
    }

    /// <summary>
    /// Cambio de una propiedad del modelo, registrado automáticamente por <see cref="SceneChangeTracker"/>.
    /// Los cambios seguidos de la misma propiedad (p. ej. cada tecla al escribir un nombre) se funden en uno.
    /// </summary>
    internal sealed class PropertyChangeCommand(SceneService scene, object target, PropertyInfo property, object? oldValue, object? newValue, Entity? owner, string description) : IMergeableCommand {
        internal static readonly TimeSpan MergeWindow = TimeSpan.FromSeconds(1);

        private object?  _newValue   = newValue;
        private DateTime _lastChange = DateTime.UtcNow;

        public string Description => description;

        public void Execute() => Apply(_newValue);

        public void Undo() => Apply(oldValue);

        public bool TryMerge(IUndoableCommand next) {
            if (next is not PropertyChangeCommand other) return false;
            if (!ReferenceEquals(other.Target, target) || other.Property != property) return false;
            if (other._lastChange - _lastChange > MergeWindow) return false;

            _newValue   = other._newValue;
            _lastChange = other._lastChange;
            return true;
        }

        private object Target => target;
        private PropertyInfo Property => property;

        private void Apply(object? value) {
            property.SetValue(target, value);
            if (owner is not null && scene.ActiveScene.Contains(owner)) scene.SelectedEntity = owner;
        }
    }
}
