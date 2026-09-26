using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Metadata;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Media;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Core.Scene;
using Somatic.Core.Scene.Components;

namespace Somatic.Controls.Inspector {
    // ComponentEditor ------------------------------------------------------------------------------
    // Base de todos los editores de componente del inspector. Pone el "chrome" común (ver
    // InspectorTheme.axaml): cabecera con desplegable, casilla de activado, icono, título y botón de
    // quitar; el cuerpo es el Content, que cada editor concreto rellena con sus propiedades:
    //
    //   <inspector:ComponentEditor x:Class="...LightComponentEditor" x:DataType="components:LightComponent">
    //       <StackPanel> ...PropertyRow... </StackPanel>
    //   </inspector:ComponentEditor>
    //
    // El DataContext es el propio Component. Título e icono los asigna ComponentEditorRegistry.
    // ---------------------------------------------------------------------------------------------
    [TemplatePart("PART_RemoveButton", typeof(Button))]
    public class ComponentEditor : ContentControl {
        public static readonly StyledProperty<string?> HeaderProperty =
            AvaloniaProperty.Register<ComponentEditor, string?>(nameof(Header));

        public static readonly StyledProperty<Geometry?> IconProperty =
            AvaloniaProperty.Register<ComponentEditor, Geometry?>(nameof(Icon));

        public static readonly StyledProperty<bool> IsExpandedProperty =
            AvaloniaProperty.Register<ComponentEditor, bool>(nameof(IsExpanded), true);

        private Button? _removeButton;

        public string? Header {
            get => GetValue(HeaderProperty);
            set => SetValue(HeaderProperty, value);
        }

        public Geometry? Icon {
            get => GetValue(IconProperty);
            set => SetValue(IconProperty, value);
        }

        public bool IsExpanded {
            get => GetValue(IsExpandedProperty);
            set => SetValue(IsExpandedProperty, value);
        }

        public Component? Component => DataContext as Component;

        // Los editores derivados comparten la plantilla de la base.
        protected override Type StyleKeyOverride => typeof(ComponentEditor);

        protected override void OnApplyTemplate(TemplateAppliedEventArgs e) {
            base.OnApplyTemplate(e);

            if (_removeButton is not null) _removeButton.Click -= OnRemoveClicked;
            _removeButton = e.NameScope.Find<Button>("PART_RemoveButton");
            if (_removeButton is not null) _removeButton.Click += OnRemoveClicked;
        }

        /// <summary>Quita el componente de su entidad (con deshacer). Los editores pueden sobrescribirlo (p. ej. para pedir confirmación).</summary>
        protected virtual void OnRemoveRequested() {
            if (Component is not { } component) return;

            ISceneService? scene = App.Services?.GetService<ISceneService>();
            if (scene is not null) scene.RemoveComponent(component);
            else component.Owner?.RemoveComponent(component);
        }

        private void OnRemoveClicked(object? sender, RoutedEventArgs e) => OnRemoveRequested();
    }
}
