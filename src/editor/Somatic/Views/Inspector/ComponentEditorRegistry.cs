using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Somatic.Controls.Inspector;
using Somatic.Converters;
using Somatic.Core.Scene.Components;
using Somatic.Localization;

namespace Somatic.Views.Inspector {
    /// <summary>Todo lo que el inspector necesita saber de un tipo de componente.</summary>
    public sealed record ComponentDescriptor(Type ComponentType, string TitleKey, string IconKey, Func<Component> Create, Func<ComponentEditor> CreateEditor) {
        public string Title => LocalizeExtension.Get(TitleKey);

        /// <summary><c>false</c> para los componentes que ya trae toda entidad y no se pueden añadir a mano.</summary>
        public bool CanBeAdded { get; init; } = true;
    }

    /// <summary>
    /// Registro de componentes conocidos por el editor. Para dar soporte a un componente nuevo: crear su
    /// editor derivando de <see cref="ComponentEditor"/> y añadir aquí una entrada.
    /// </summary>
    public static class ComponentEditorRegistry {
        public static IReadOnlyList<ComponentDescriptor> Descriptors { get; } = [
            new(typeof(TransformComponent),    "ComponentTransform",    "IconEntity",    () => new TransformComponent(),    () => new TransformComponentEditor())    { CanBeAdded = false },
            new(typeof(CameraComponent),       "ComponentCamera",       "IconCamera",    () => new CameraComponent(),       () => new CameraComponentEditor()),
            new(typeof(LightComponent),        "ComponentLight",        "IconLightbulb", () => new LightComponent(),        () => new LightComponentEditor()),
            new(typeof(MeshRendererComponent), "ComponentMeshRenderer", "IconCube",      () => new MeshRendererComponent(), () => new MeshRendererComponentEditor())
        ];

        public static ComponentDescriptor? Find(Type componentType) => Descriptors.FirstOrDefault(d => d.ComponentType == componentType);
    }

    /// <summary>Plantilla de datos del stack de componentes: elige el editor según el tipo del componente.</summary>
    public sealed class ComponentEditorLocator : IDataTemplate {
        public Control? Build(object? param) {
            if (param is not Component component) return null;

            ComponentDescriptor? descriptor = ComponentEditorRegistry.Find(component.GetType());
            if (descriptor is null) {
                // Componente sin editor: al menos se ve la cabecera con su nombre de tipo.
                return new ComponentEditor { Header = component.GetType().Name, Icon = IconResourceConverter.Find("IconDocument") };
            }

            ComponentEditor editor = descriptor.CreateEditor();
            editor.Header = descriptor.Title;
            editor.Icon   = IconResourceConverter.Find(descriptor.IconKey);
            return editor;
        }

        public bool Match(object? data) => data is Component;
    }
}
