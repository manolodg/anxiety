using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Core.Scene;
using Somatic.Localization;
using Somatic.Views.Inspector;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Text.Json.Serialization;

namespace Somatic.ViewModels.Panels {
    // Igual que la jerarquía: se recrea al deserializar el layout, así que nada de esto va al JSON.
    public sealed class InspectorToolViewModel : Tool {
        private readonly ISceneService? _scene;
        private Entity? _entity;
        private IReadOnlyList<MenuOption> _addComponentOptions = [];

        /// <summary>Entidad seleccionada; su lista de componentes se pinta como un stack de editores.</summary>
        [JsonIgnore] public Entity? Entity => _entity;

        [JsonIgnore] public bool HasEntity => _entity is not null;

        [JsonIgnore] public string NoSelectionText { get; } = LocalizeExtension.Get("NoEntitySelected");

        /// <summary>Componentes que aún no tiene la entidad seleccionada.</summary>
        [JsonIgnore]
        public IReadOnlyList<MenuOption> AddComponentOptions {
            get => _addComponentOptions;
            private set {
                if (SetProperty(ref _addComponentOptions, value)) OnPropertyChanged(nameof(CanAddComponent));
            }
        }

        [JsonIgnore] public bool CanAddComponent => _addComponentOptions.Count > 0;

        [JsonIgnore] public IRelayCommand<ComponentDescriptor> AddComponentCommand { get; }

        public InspectorToolViewModel() {
            _scene = App.Services?.GetService<ISceneService>();
            AddComponentCommand = new RelayCommand<ComponentDescriptor>(descriptor => {
                if (descriptor is not null && _entity is not null) _scene?.AddComponent(_entity, descriptor.Create());
            });

            if (_scene is null) return;
            _scene.PropertyChanged += OnScenePropertyChanged;
            SetEntity(_scene.SelectedEntity);
        }

        private void OnScenePropertyChanged(object? sender, PropertyChangedEventArgs e) {
            if (e.PropertyName == nameof(ISceneService.SelectedEntity)) SetEntity(_scene?.SelectedEntity);
        }

        private void SetEntity(Entity? entity) {
            if (_entity == entity) return;

            if (_entity is not null) ((INotifyCollectionChanged)_entity.Components).CollectionChanged -= OnComponentsChanged;
            _entity = entity;
            if (_entity is not null) ((INotifyCollectionChanged)_entity.Components).CollectionChanged += OnComponentsChanged;

            OnPropertyChanged(nameof(Entity));
            OnPropertyChanged(nameof(HasEntity));
            RefreshAddComponentOptions();
        }

        private void OnComponentsChanged(object? sender, NotifyCollectionChangedEventArgs e) => RefreshAddComponentOptions();

        private void RefreshAddComponentOptions() {
            AddComponentOptions = _entity is null
                ? []
                : ComponentEditorRegistry.Descriptors
                    .Where(d => d.CanBeAdded && !_entity.HasComponent(d.ComponentType))
                    .Select(d => new MenuOption(d.Title, d.IconKey, AddComponentCommand, d))
                    .ToList();
        }
    }
}
