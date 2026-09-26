using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.Logging;
using Somatic.Core.History;
using Somatic.Core.Localization;
using Somatic.Core.Scene;
using Somatic.Core.Scene.Components;
using System.Numerics;

namespace Somatic.Infrastructure.Scene {
    public sealed class SceneService : ObservableObject, ISceneService {
        private readonly ILocalizationService _localization;
        private readonly IUndoService _undo;
        private readonly ILogger<SceneService> _logger;
        private readonly SceneChangeTracker _tracker;
        private Entity? _selectedEntity;

        public SceneGraph ActiveScene { get; } = new SceneGraph();

        public Entity? SelectedEntity {
            get => _selectedEntity;
            set => SetProperty(ref _selectedEntity, value);
        }

        public SceneService(ILocalizationService localization, IUndoService undo, ILogger<SceneService> logger) {
            _localization = localization;
            _undo = undo;
            _logger = logger;

            _tracker = new SceneChangeTracker(this, undo, owner => Describe("HistoryEdit", owner.Name));

            PopulateDefaultScene();
            // La escena de partida no es algo que el usuario deba poder deshacer.
            _undo.Clear();
        }

        public Entity CreateEntity(EntityKind kind, Entity? parent = null) {
            Entity entity = EntityFactory.Create(kind, MakeUniqueName(DefaultName(kind), parent?.Children ?? (IReadOnlyList<Entity>)ActiveScene.RootEntities));

            _undo.Execute(new CreateEntityCommand(this, entity, parent, Describe("HistoryCreate", entity.Name)));
            _logger.LogDebug("Entidad {Name} ({Kind}) creada bajo {Parent}", entity.Name, kind, parent?.Name ?? "<raíz>");
            return entity;
        }

        public void DeleteEntity(Entity entity) {
            if (!ActiveScene.Contains(entity)) return;

            _undo.Execute(new DeleteEntityCommand(this, entity, Describe("HistoryDelete", entity.Name)));
            _logger.LogDebug("Entidad {Name} eliminada", entity.Name);
        }

        public bool MoveEntity(Entity entity, Entity? newParent, int index = -1) {
            if (!ActiveScene.TryResolveMove(entity, newParent, index, out int finalIndex)) return false;

            Entity? oldParent = entity.Parent;
            int oldIndex = ActiveScene.IndexOf(entity);
            _undo.Execute(new MoveEntityCommand(this, entity, oldParent, oldIndex, newParent, finalIndex, Describe("HistoryMove", entity.Name)));

            _logger.LogDebug("Entidad {Name} movida bajo {Parent}", entity.Name, newParent?.Name ?? "<raíz>");
            return true;
        }

        // El cambio de Name lo registra SceneChangeTracker como cualquier otra propiedad.
        public bool RenameEntity(Entity entity, string name) {
            string trimmed = name.Trim();
            if (trimmed.Length == 0 || trimmed == entity.Name) return false;

            entity.Name = trimmed;
            return true;
        }

        public bool AddComponent(Entity entity, Component component) {
            if (component.Owner is not null || entity.HasComponent(component.GetType())) return false;

            _undo.Execute(new AddComponentCommand(this, entity, component, Describe("HistoryAddComponent", entity.Name)));
            return true;
        }

        public bool RemoveComponent(Component component) {
            if (component.Owner is not { } entity || !component.CanRemove) return false;

            _undo.Execute(new RemoveComponentCommand(this, entity, component, Describe("HistoryRemoveComponent", entity.Name)));
            return true;
        }

        internal string Describe(string key, string entityName) => string.Format(_localization.GetString(key), entityName);

        // Escena nueva: una cámara y una luz direccional, como punto de partida habitual.
        private void PopulateDefaultScene() {
            Entity camera = CreateEntity(EntityKind.Camera);
            camera.Transform.Position = new Vector3(0.0f, 1.0f, -10.0f);

            Entity light = CreateEntity(EntityKind.Light);
            light.Transform.Rotation = new Vector3(50.0f, -30.0f, 0.0f);
            if (light.GetComponent<LightComponent>() is { } lightComponent) lightComponent.Type = LightType.Directional;

            SelectedEntity = null;
        }

        private string DefaultName(EntityKind kind) => _localization.GetString($"EntityKind{kind}");

        // "Luz", "Luz (1)", "Luz (2)"... sólo se compara con las hermanas: en ramas distintas pueden repetirse.
        private static string MakeUniqueName(string baseName, IReadOnlyList<Entity> siblings) {
            HashSet<string> taken = siblings.Select(e => e.Name).ToHashSet(StringComparer.OrdinalIgnoreCase);
            if (!taken.Contains(baseName)) return baseName;

            for (int i = 1; ; ++i) {
                string candidate = $"{baseName} ({i})";
                if (!taken.Contains(candidate)) return candidate;
            }
        }
    }
}
