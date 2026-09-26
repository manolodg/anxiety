using Somatic.Core.History;
using Somatic.Core.Scene;
using Somatic.Core.Scene.Components;
using System.Collections.Concurrent;
using System.Collections.Specialized;
using System.ComponentModel;
using Component = Somatic.Core.Scene.Components.Component;
using System.Reflection;

namespace Somatic.Infrastructure.Scene {
    // SceneChangeTracker ---------------------------------------------------------------------------
    // Convierte en entradas del historial cualquier cambio de propiedad de entidades y componentes,
    // venga de donde venga (inspector, renombrado en la jerarquía, código...). Así un editor de
    // componente nuevo tiene deshacer sin escribir nada.
    //
    //   PropertyChanging -> se guarda el valor anterior
    //   PropertyChanged  -> si cambió, se apila un PropertyChangeCommand (ya aplicado)
    //
    // Se ignoran: los cambios que provoca el propio historial (IUndoService.IsApplying), las propiedades
    // con [UndoIgnore] y las que no tienen setter público.
    //
    // Se suscribe a toda entidad/componente que entra en la escena y no se desuscribe nunca: lo que sale
    // puede volver al deshacer, y fuera de la escena nadie lo edita.
    // ---------------------------------------------------------------------------------------------
    internal sealed class SceneChangeTracker {
        private static readonly ConcurrentDictionary<(Type, string), PropertyInfo?> PropertyCache = new();

        private readonly SceneService _scene;
        private readonly IUndoService _undo;
        private readonly Func<Entity, string> _describe;
        private readonly HashSet<object> _tracked = new(ReferenceEqualityComparer.Instance);
        private readonly Dictionary<(object Target, string Property), object?> _pending = new();

        public SceneChangeTracker(SceneService scene, IUndoService undo, Func<Entity, string> describe) {
            _scene    = scene;
            _undo     = undo;
            _describe = describe;

            ((INotifyCollectionChanged)scene.ActiveScene.RootEntities).CollectionChanged += OnEntitiesChanged;
            foreach (Entity entity in scene.ActiveScene.RootEntities) Track(entity);
        }

        private void Track(Entity entity) {
            if (!_tracked.Add(entity)) return;

            entity.PropertyChanging += OnPropertyChanging;
            entity.PropertyChanged  += OnPropertyChanged;
            ((INotifyCollectionChanged)entity.Children).CollectionChanged   += OnEntitiesChanged;
            ((INotifyCollectionChanged)entity.Components).CollectionChanged += OnComponentsChanged;

            foreach (Component component in entity.Components) Track(component);
            foreach (Entity child in entity.Children) Track(child);
        }

        private void Track(Component component) {
            if (!_tracked.Add(component)) return;

            component.PropertyChanging += OnPropertyChanging;
            component.PropertyChanged  += OnPropertyChanged;
        }

        private void OnEntitiesChanged(object? sender, NotifyCollectionChangedEventArgs e) {
            if (e.NewItems is null) return;
            foreach (Entity entity in e.NewItems.OfType<Entity>()) Track(entity);
        }

        private void OnComponentsChanged(object? sender, NotifyCollectionChangedEventArgs e) {
            if (e.NewItems is null) return;
            foreach (Component component in e.NewItems.OfType<Component>()) Track(component);
        }

        private void OnPropertyChanging(object? sender, PropertyChangingEventArgs e) {
            if (sender is null || e.PropertyName is null || _undo.IsApplying) return;
            if (FindTrackedProperty(sender.GetType(), e.PropertyName) is not { } property) return;

            _pending[(sender, e.PropertyName)] = property.GetValue(sender);
        }

        private void OnPropertyChanged(object? sender, PropertyChangedEventArgs e) {
            if (sender is null || e.PropertyName is null) return;
            if (!_pending.Remove((sender, e.PropertyName), out object? oldValue) || _undo.IsApplying) return;
            if (FindTrackedProperty(sender.GetType(), e.PropertyName) is not { } property) return;

            object? newValue = property.GetValue(sender);
            if (Equals(oldValue, newValue)) return;

            Entity? owner = sender as Entity ?? (sender as Component)?.Owner;
            string description = owner is null ? e.PropertyName : _describe(owner);
            if (sender is Entity && e.PropertyName == nameof(Entity.Name)) description = _scene.Describe("HistoryRename", oldValue as string ?? string.Empty);

            _undo.Record(new PropertyChangeCommand(_scene, sender, property, oldValue, newValue, owner, description));
        }

        private static PropertyInfo? FindTrackedProperty(Type type, string name) =>
            PropertyCache.GetOrAdd((type, name), static key => {
                PropertyInfo? property = key.Item1.GetProperty(key.Item2, BindingFlags.Public | BindingFlags.Instance);
                if (property is null || property.SetMethod is not { IsPublic: true } || property.GetIndexParameters().Length > 0) return null;
                return property.IsDefined(typeof(UndoIgnoreAttribute), inherit: true) ? null : property;
            });
    }
}
