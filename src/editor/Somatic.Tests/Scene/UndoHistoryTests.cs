using Microsoft.Extensions.Logging.Abstractions;
using Somatic.Core.Localization;
using Somatic.Core.Scene;
using Somatic.Core.Scene.Components;
using Somatic.Infrastructure.History;
using Somatic.Infrastructure.Scene;
using System.Globalization;
using System.Numerics;

namespace Somatic.Tests.Scene {
    public sealed class UndoHistoryTests {
        // Devuelve la clave con un hueco para el nombre, así las descripciones son predecibles.
        private sealed class KeyLocalization : ILocalizationService {
            public CultureInfo CurrentCulture => CultureInfo.InvariantCulture;
            public string GetString(string key) => key.StartsWith("History", StringComparison.Ordinal) ? key + " {0}" : key;
            public void SetCulture(CultureInfo culture) { }
        }

        private readonly UndoService  _undo  = new(NullLogger<UndoService>.Instance);
        private readonly SceneService _scene;

        public UndoHistoryTests() {
            _scene = new SceneService(new KeyLocalization(), _undo, NullLogger<SceneService>.Instance);
        }

        private string RootNames => string.Join(",", _scene.ActiveScene.RootEntities.Select(e => e.Name));

        [Fact]
        public void DefaultScene_IsNotUndoable() {
            Assert.Equal(2, _scene.ActiveScene.RootEntities.Count);
            Assert.False(_undo.CanUndo);
        }

        [Fact]
        public void Create_UndoRedo() {
            Entity group = _scene.CreateEntity(EntityKind.Group);
            Assert.Equal("HistoryCreate EntityKindGroup", _undo.UndoDescription);
            Assert.Same(group, _scene.SelectedEntity);

            _undo.Undo();
            Assert.False(_scene.ActiveScene.Contains(group));
            Assert.Null(_scene.SelectedEntity);

            _undo.Redo();
            Assert.True(_scene.ActiveScene.Contains(group));
            Assert.Same(group, _scene.SelectedEntity);
        }

        [Fact]
        public void Delete_Undo_RestoresParentAndPosition() {
            Entity group  = _scene.CreateEntity(EntityKind.Group);
            Entity first  = _scene.CreateEntity(EntityKind.Mesh, group);
            Entity second = _scene.CreateEntity(EntityKind.Light, group);
            Entity third  = _scene.CreateEntity(EntityKind.Camera, group);
            _scene.SelectedEntity = second;

            _scene.DeleteEntity(second);
            Assert.Equal([first, third], group.Children);
            Assert.Same(group, _scene.SelectedEntity);

            _undo.Undo();
            Assert.Equal([first, second, third], group.Children);
            Assert.Same(second, _scene.SelectedEntity);
        }

        [Fact]
        public void Move_Undo_RestoresOriginalOrderAndParent() {
            Entity group = _scene.CreateEntity(EntityKind.Group);
            string before = RootNames;
            Entity camera = _scene.ActiveScene.RootEntities[0];

            Assert.True(_scene.MoveEntity(camera, group));
            Assert.Same(group, camera.Parent);

            _undo.Undo();
            Assert.Null(camera.Parent);
            Assert.Equal(before, RootNames);

            _undo.Redo();
            Assert.Same(group, camera.Parent);
        }

        [Fact]
        public void Move_WithinSameList_UndoRestoresIndex() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            _scene.CreateEntity(EntityKind.Empty);
            string before = RootNames;

            Assert.True(_scene.MoveEntity(camera, null, 3)); // al final
            Assert.NotEqual(before, RootNames);

            _undo.Undo();
            Assert.Equal(before, RootNames);
        }

        [Fact]
        public void PropertyEdit_IsRecordedAutomatically() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            CameraComponent component = camera.GetComponent<CameraComponent>()!;

            component.FieldOfView = 90.0f;
            Assert.True(_undo.CanUndo);
            Assert.Equal("HistoryEdit " + camera.Name, _undo.UndoDescription);

            _undo.Undo();
            Assert.Equal(60.0f, component.FieldOfView);
            Assert.Same(camera, _scene.SelectedEntity);

            _undo.Redo();
            Assert.Equal(90.0f, component.FieldOfView);
        }

        [Fact]
        public void ConsecutiveEdits_OfSameProperty_MergeIntoOneStep() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            string original = camera.Name;

            // Simula teclear en la caja del nombre: un cambio por pulsación.
            camera.Name = "S";
            camera.Name = "So";
            camera.Name = "Sol";

            _undo.Undo();
            Assert.Equal(original, camera.Name);
            Assert.False(_undo.CanUndo);
        }

        [Fact]
        public void EditsOfDifferentProperties_AreSeparateSteps() {
            TransformComponent transform = _scene.ActiveScene.RootEntities[0].Transform;
            Vector3 originalPosition = transform.Position;

            transform.Position = new Vector3(1, 2, 3);
            transform.Scale    = new Vector3(2, 2, 2);

            _undo.Undo();
            Assert.Equal(Vector3.One, transform.Scale);
            Assert.Equal(new Vector3(1, 2, 3), transform.Position);

            _undo.Undo();
            Assert.Equal(originalPosition, transform.Position);
        }

        [Fact]
        public void EditAfterUndo_DoesNotMergeAndClearsRedo() {
            TransformComponent transform = _scene.ActiveScene.RootEntities[0].Transform;

            transform.Position = new Vector3(1, 0, 0);
            transform.Position = new Vector3(2, 0, 0); // se funde con el anterior
            _undo.Undo();
            transform.Position = new Vector3(5, 0, 0);

            Assert.False(_undo.CanRedo);
            _undo.Undo();
            Assert.Equal(new Vector3(0, 1, -10), transform.Position);
        }

        [Fact]
        public void EditorOnlyState_IsIgnored() {
            Entity group = _scene.CreateEntity(EntityKind.Group);
            _undo.Clear();

            group.IsExpanded = !group.IsExpanded;

            Assert.False(_undo.CanUndo);
        }

        [Fact]
        public void ComponentAddRemove_UndoRestoresPosition() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            CameraComponent cameraComponent = camera.GetComponent<CameraComponent>()!;

            Assert.True(_scene.AddComponent(camera, new LightComponent()));
            Assert.Equal(3, camera.Components.Count);

            Assert.True(_scene.RemoveComponent(cameraComponent));
            Assert.Null(camera.GetComponent<CameraComponent>());

            _undo.Undo();
            Assert.Equal(1, camera.IndexOfComponent(cameraComponent));

            _undo.Undo();
            Assert.Null(camera.GetComponent<LightComponent>());
        }

        [Fact]
        public void Transform_CannotBeRemoved() {
            Entity camera = _scene.ActiveScene.RootEntities[0];

            Assert.False(_scene.RemoveComponent(camera.Transform));
            Assert.False(_undo.CanUndo);
        }

        [Fact]
        public void EditingComponentAddedLater_IsTracked() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            LightComponent light = new LightComponent();
            _scene.AddComponent(camera, light);

            light.Intensity = 3.0f;
            _undo.Undo();

            Assert.Equal(1.0f, light.Intensity);
        }

        [Fact]
        public void EditingEntityRestoredByUndo_IsTracked() {
            Entity mesh = _scene.CreateEntity(EntityKind.Mesh);
            _scene.DeleteEntity(mesh);
            _undo.Undo();

            mesh.IsActive = false;

            Assert.Equal("HistoryEdit " + mesh.Name, _undo.UndoDescription);
            _undo.Undo();
            Assert.True(mesh.IsActive);
        }

        [Fact]
        public void Rename_HasItsOwnDescription() {
            Entity camera = _scene.ActiveScene.RootEntities[0];
            string original = camera.Name;

            Assert.True(_scene.RenameEntity(camera, "  Principal "));
            Assert.Equal("Principal", camera.Name);
            Assert.Equal("HistoryRename " + original, _undo.UndoDescription);

            Assert.False(_scene.RenameEntity(camera, "   "));
        }

        [Fact]
        public void History_IsBounded() {
            TransformComponent transform = _scene.ActiveScene.RootEntities[0].Transform;
            _undo.Clear();

            // Alterna propiedades para que no se fundan.
            for (int i = 0; i < UndoService.MaxHistory + 50; ++i) {
                if (i % 2 == 0) transform.Position = new Vector3(i, 0, 0);
                else            transform.Scale    = new Vector3(i, 1, 1);
            }

            int steps = 0;
            while (_undo.CanUndo) {
                _undo.Undo();
                ++steps;
            }
            Assert.Equal(UndoService.MaxHistory, steps);
        }
    }
}
