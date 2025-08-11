using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.VisualTree;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Model;
using Somatic.Services;
using Somatic.ViewModels;
using System;

namespace Somatic.Controls {
    /// <summary>Componente visual base de cualquier control de componente.</summary>
    public partial class ComponentControl : UserControl {
#region Propiedades Avalonia
        /// <summary>Idiomatico de título que se ha de presentar en la barra de título.</summary>
        public static readonly StyledProperty<string> TitleProperty = AvaloniaProperty.Register<ComponentControl, string>(nameof(Title), "Empty Title");
        /// <summary>Contenido que tiene en el body el componente.</summary>
        public static readonly StyledProperty<object> InnerContentProperty = AvaloniaProperty.Register<ComponentControl, object>(nameof(InnerContent), "Default Content");
        /// <summary>ViewModel que realiza toda la gestión del componente.</summary>
        public static readonly StyledProperty<object?> InformationViewModelProperty = AvaloniaProperty.Register<ComponentControl, object?>(nameof(InformationViewModel));


        /// <summary>Indica si el control esta o no extendido.</summary>
        public static readonly StyledProperty<bool> IsExpandedProperty = AvaloniaProperty.Register<ComponentControl, bool>(nameof(IsExpanded), true, defaultBindingMode: BindingMode.TwoWay);
        /// <summary>Indica si el control esta o no bloqueado.</summary>
        public static readonly StyledProperty<bool> IsLockedProperty = AvaloniaProperty.Register<ComponentControl, bool>(nameof(IsLocked), false, defaultBindingMode: BindingMode.TwoWay);
        /// <summary>Indica si el control esta o no siendo arrastrado.</summary>
        public static readonly StyledProperty<bool> IsDraggingProperty = AvaloniaProperty.Register<ComponentControl, bool>(nameof(IsDragging), false);

        /// <summary>Idiomatico de título que se ha de presentar en la barra de título.</summary>
        public string Title {
            get => GetValue(TitleProperty);
            set {
                if (Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString(value) == null) value = "Empty Title";
                SetValue(TitleProperty, value);
            }
        }
        /// <summary>Contenido que tiene en el body el componente.</summary>
        public object InnerContent {
            get => GetValue(InnerContentProperty);
            set => SetValue(InnerContentProperty, value);
        }
        /// <summary>ViewModel que realiza toda la gestión del componente.</summary>
        public object? InformationViewModel {
            get => GetValue(InformationViewModelProperty);
            set => SetValue(InformationViewModelProperty, value);
        }

        /// <summary>Indica si el control está o no extendido.</summary>
        public bool IsExpanded {
            get => GetValue(IsExpandedProperty);
            set => SetValue(IsExpandedProperty, value);
        }
        /// <summary>Indica si el control esta o no bloqueado.</summary>
        public bool IsLocked {
            get => GetValue(IsLockedProperty);
            set => SetValue(IsLockedProperty, value);
        }
        /// <summary>Indica si el control esta o no siendo arrastrado.</summary>
        public bool IsDragging {
            get => GetValue(IsDraggingProperty);
            set => SetValue(IsDraggingProperty, value);
        }
        /// <summary>Componente vinculado a este control.</summary>
        public BaseComponent? Component => _component;
#endregion

#region Campos
        // Componente que contiene el control.
        private BaseComponent? _component;

        // Borde de la barra de título.
        private Border? _headerBorder;
        // Contenedor del panel principal
        private Panel? _mainContainer;
#endregion

#region Constructor
        /// <summary>Crea una instancia del <see cref="ComponentControl"/>.</summary>
        public ComponentControl() {
            InitializeComponent();

            DataContextChanged += OnDataContextChanged;
            CreateVisuals();
        }
#endregion

#region Eventos de ventana
        /// <summary>El valor del data context lo asignamos al componente.</summary>
        protected void OnDataContextChanged(object? sender, EventArgs e) => _component = DataContext as BaseComponent;

        /// <summary>Capturamos el control para poder desplazarlo.</summary>
        private void OnHeaderPointerPressed(object? sender, PointerPressedEventArgs e) {
            if (_component != null && e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) {
                IsDragging = true;

                this.Classes.Add("dragging");

                GetComponentListControl()?.CreatePreview(e.GetPosition(_mainContainer), e.GetPosition(this).Y, _component);
            }
        }
        /// <summary>Liberamos la captura del control.</summary>
        private void OnHeaderPointerReleased(object? sender, PointerReleasedEventArgs e) {
            if (IsDragging) {
                IsDragging = false;

                this.Classes.Remove("dragging");

                GetComponentListControl()?.RemovePreview();
            }
        }
        /// <summary>Movimiento del ratón cuando se arrastra.</summary>
        protected override void OnPointerMoved(PointerEventArgs e) {
            base.OnPointerMoved(e);

            if (IsDragging && _component != null) {
                GetComponentListControl()?.MovePreview(e.GetPosition(_mainContainer));
            }
        }
#endregion

#region Eventos de usuario
        /// <summary>Expande o contrae el cuerpo del componente.</summary>
        private void ToggleExpanded(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => IsExpanded = !IsExpanded;
        /// <summary>Bloqueo o desbloquea el componente.</summary>
        private void ToggleLocked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => IsLocked = !IsLocked;

        [RelayCommand]
        private void Up() => GetComponentListControl()?.MoveUp(_component!);
        [RelayCommand]
        private void Down() => GetComponentListControl()?.MoveDown(_component!);
        [RelayCommand]
        private void Remove() => GetComponentListControl()?.Remove(_component!);
#endregion

#region Métodos
        /// <summary>Obtenemos el ViewModel (InformationViewModel) que realiza la gestión completa de componentes.</summary>
        public InformationViewModel? GetInformationViewModel() => GetValue(InformationViewModelProperty) as InformationViewModel;
        /// <summary>Obtenemos el ComponentListControl sobre el que estamos trabajando.</summary>
        public ComponentListControl? GetComponentListControl() => (GetValue(InformationViewModelProperty) as InformationViewModel)?.ComponentListControl;

        /// <summary>Captura la información de los elementos que vamos a manipular.</summary>
        private void CreateVisuals() {
            FindMainContainer();

            _headerBorder = this.FindControl<Border>("TitleBar");
            if (_headerBorder != null) {
                _headerBorder.PointerPressed += OnHeaderPointerPressed;
                _headerBorder.PointerReleased += OnHeaderPointerReleased;
            }
        }

        /// <summary>Realiza la búsqueda del contenedor de todos los componentes.</summary>
        private void FindMainContainer() {
            StyledElement? current = this.Parent;
            while (current != null) {
                Panel? panel = current is Panel ? current as Panel : null;
                if (current is Grid || current is Canvas || current is Panel) {
                    _mainContainer = panel;
                    return;
                }
                current = (current as Visual)?.GetVisualParent();
            }

            // Fallback: usar el TopLevel
            TopLevel? topLevel = TopLevel.GetTopLevel(this);
            if (topLevel?.Content is Panel p) _mainContainer = p;
        }
#endregion
    }
}
