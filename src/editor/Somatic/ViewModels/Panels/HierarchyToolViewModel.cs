using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Converters;
using Somatic.Core.Scene;
using Somatic.Localization;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Text.Json.Serialization;

namespace Somatic.ViewModels.Panels {
    // El Dock serializa este objeto con el layout del workspace y lo recrea con el constructor sin
    // parámetros, así que el estado de escena se toma del contenedor y se excluye del JSON.
    public sealed class HierarchyToolViewModel : Tool {
        private readonly ISceneService? _scene;

        [JsonIgnore] public ReadOnlyObservableCollection<Entity>? RootEntities => _scene?.ActiveScene.RootEntities;

        [JsonIgnore]
        public Entity? SelectedEntity {
            get => _scene?.SelectedEntity;
            set {
                if (_scene is not null) _scene.SelectedEntity = value;
            }
        }

        [JsonIgnore] public bool HasSelection => SelectedEntity is not null;

        /// <summary>Opciones de "Crear entidad" (en la raíz).</summary>
        [JsonIgnore] public IReadOnlyList<MenuOption> CreateRootOptions { get; }

        /// <summary>Opciones de "Crear hija" (bajo la entidad seleccionada).</summary>
        [JsonIgnore] public IReadOnlyList<MenuOption> CreateChildOptions { get; }

        [JsonIgnore] public IRelayCommand<EntityKind> CreateRootEntityCommand  { get; }
        [JsonIgnore] public IRelayCommand<EntityKind> CreateChildEntityCommand { get; }
        [JsonIgnore] public IRelayCommand             DeleteEntityCommand      { get; }
        [JsonIgnore] public IRelayCommand             RenameEntityCommand      { get; }

        /// <summary>La vista lo escucha para abrir la edición en línea del nombre de esa entidad.</summary>
        public event EventHandler<Entity>? RenameRequested;

        public HierarchyToolViewModel() {
            _scene = App.Services?.GetService<ISceneService>();
            if (_scene is not null) _scene.PropertyChanged += OnScenePropertyChanged;

            CreateRootEntityCommand  = new RelayCommand<EntityKind>(kind => _scene?.CreateEntity(kind));
            CreateChildEntityCommand = new RelayCommand<EntityKind>(kind => _scene?.CreateEntity(kind, SelectedEntity), _ => HasSelection);
            DeleteEntityCommand      = new RelayCommand(() => {
                if (SelectedEntity is { } entity) _scene?.DeleteEntity(entity);
            }, () => HasSelection);
            RenameEntityCommand      = new RelayCommand(() => {
                if (SelectedEntity is { } entity) RenameRequested?.Invoke(this, entity);
            }, () => HasSelection);

            CreateRootOptions  = BuildCreateOptions(CreateRootEntityCommand);
            CreateChildOptions = BuildCreateOptions(CreateChildEntityCommand);
        }

        private static IReadOnlyList<MenuOption> BuildCreateOptions(IRelayCommand<EntityKind> command) =>
            Enum.GetValues<EntityKind>()
                .Select(kind => new MenuOption(LocalizeExtension.Get($"EntityKind{kind}"), EntityIconConverter.IconKeyFor(kind), command, kind))
                .ToList();

        public bool CanMoveEntity(Entity entity, Entity? newParent) => SceneGraph.CanMove(entity, newParent);

        public bool MoveEntity(Entity entity, Entity? newParent, int index) => _scene?.MoveEntity(entity, newParent, index) ?? false;

        public bool RenameEntity(Entity entity, string name) => _scene?.RenameEntity(entity, name) ?? false;

        private void OnScenePropertyChanged(object? sender, PropertyChangedEventArgs e) {
            if (e.PropertyName != nameof(ISceneService.SelectedEntity)) return;

            OnPropertyChanged(nameof(SelectedEntity));
            OnPropertyChanged(nameof(HasSelection));
            CreateChildEntityCommand.NotifyCanExecuteChanged();
            DeleteEntityCommand.NotifyCanExecuteChanged();
            RenameEntityCommand.NotifyCanExecuteChanged();
        }
    }
}
