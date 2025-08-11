using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Somatic.Extensions;
using Somatic.Model;
using Somatic.ViewModels;
using Somatic.Views;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Somatic.Controls {
    /// <summary>Control de lista de componentes.</summary>
    public partial class ComponentListControl : UserControl {
#region Campos
        // Desplazamiento que hay que añadir según la posición del puntero en la barra de título
        private double _offset;
        // Componente que mostramos y que arrastramos
        private ComponentControl? _preview;
        // Componente sobre el que se encuentra el que arrastramos
        private ComponentControl? _closest = null;
        // Componente que estamos manejando
        private BaseComponent? _component = null;
        // Indica si se inserta antes o detras
        private bool _insertBefore = false;
        // Componentes que soporta este componente
        private ObservableCollection<BaseComponent> _components = null!;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase <see cref="ComponentListControl"/>.</summary>
        public ComponentListControl() {
            InitializeComponent();

            if (Design.IsDesignMode) {
                DataContext = new InformationViewModel {
                    Components = [
                        new TransformComponent { Name = "Transform Component" },
                        new ScriptComponent { Name = "Script Component", Parameters = [ new ScriptParameter { Name = "Parametro 1" }] }
                    ]
                };
            }

            this.DataContextChanged += (s, e) => {
                (DataContext as InformationViewModel)!.ComponentListControl = this;
                _components = (DataContext as InformationViewModel)!.Components;
            };
        }
#endregion

#region Métodos Públicos
        /// <summary>Crea la previsualización del control que se genera al capturarlo.</summary>
        /// <param name="position">Punto en el que se ha producido la captura sobre el panel.</param>
        /// <param name="offset">Desplazamiento en el título.</param>
        /// <param name="component">Componente que se ha capturado.</param>
        public void CreatePreview(Point position, double offset, BaseComponent component) {
            _component = component;
            _offset = offset;

            _preview = GetComponent(component);
            _preview.InformationViewModel = DataContext;
            _preview.RenderTransform = new ScaleTransform(1.01, 1.01);
            _preview.Opacity = 0.6;

            _preview.Margin = new Thickness(0, position.Y - offset, 0, 0);

            PART_Overlay.Children.Add( _preview );
        }
        /// <summary>Borra la previsualización y se ejecuta el soltado del control.</summary>
        public void RemovePreview() {
            if (_preview != null ) PART_Overlay.Children.Remove(_preview);
            if (_closest != null) {
                _closest.PART_DropBefore.Opacity = 0;
                _closest.PART_DropAfter.Opacity = 0;

                if (_component != null && _closest != null) {
                    int index = _closest.Component!.Order;

                    if (_insertBefore) {
                        MoveUp(_component, index);
                    } else {
                        MoveDown(_component, index);
                    }
                }
            }
        }
        /// <summary>Realiza el movimiento del control según se arrastra el ratón.</summary>
        /// <param name="position">Posición nueva del arrastre.</param>
        public void MovePreview(Point position) {
            _preview!.Margin = new Thickness(0, position.Y - _offset, 0, 0);

            UpdateDropIndicator(position);
        }
        /// <summary>Subir un componente.</summary>
        /// <param name="component">Componente a subir.</param>
        public void MoveUp(BaseComponent component, int index = -1) {
            if (index < 0) index = component.Order - 1;
            if (index < 0) index = 0;

            //ComponentControl visual = PART_ItemsControl.FindChilds<ComponentControl>().First(x => x.Component == component);
            int actual = _components.IndexOf(component);
            _components.RemoveAt(actual);
            _components.Insert(index, component);

            RefreshOrders();

            (DataContext as InformationViewModel)?.UpdateComponent();
        }
        /// <summary>Bajar un componente.</summary>
        /// <param name="component">Componente a bajar.</param>
        public void MoveDown(BaseComponent component, int index = -1) {
            if (index < 0) index = component.Order + 1;
            if (index < 0) index = 0;

            //ComponentControl visual = PART_ItemsControl.FindChilds<ComponentControl>().First(x => x.Component == component);
            int actual = _components.IndexOf(component);
            _components.RemoveAt(actual);
            _components.Insert(index, component);

            RefreshOrders();

            (DataContext as InformationViewModel)?.UpdateComponent();
        }
        /// <summary>Realiza el borrado del componente indicado.</summary>
        /// <param name="component">Componente a borrar.</param>
        public void Remove(BaseComponent component) {
            (DataContext as InformationViewModel)?.RemoveComponent(component);

            RefreshOrders();
        }
#endregion

#region Métodos Privados
        /// <summary>Actualiza la visualización del indicador y el control sobre el que nos encontramos.</summary>
        private void UpdateDropIndicator(Point position) {
            ComponentControl? beforeComponent = _closest;
            if (beforeComponent != null) {
                beforeComponent.PART_DropBefore.Opacity = 0;
                beforeComponent.PART_DropAfter.Opacity = 0;
            }

            double height = position.Y;
            _insertBefore = false;
            _closest = null;

            foreach (ComponentControl component in PART_ItemsControl.FindChilds<ComponentControl>()) {
                height -= component.DesiredSize.Height;

                if (component.Classes.Contains("dragging")) continue;

                if (height <= 0) {
                    _closest = component;
                    _insertBefore = (component.DesiredSize.Height + height) < (component.DesiredSize.Height / 2);
                    break;
                }
            }

            if (_closest != null) ShowDropIndicator();
        }
        /// <summary>Pone el indicador al principio o al final según se haya detectado.</summary>
        private void ShowDropIndicator() {
            if (_insertBefore) {
                _closest!.PART_DropBefore.Opacity = 1.0d;
            } else {
                _closest!.PART_DropAfter.Opacity = 1.0d;
            }
        }
        /// <summary>Refresca la ordenación de elementos.</summary>
        private void RefreshOrders() {
            int index = 0;
            foreach (BaseComponent component in _components) {
                component.Order = index++;
            }
        }
        /// <summary>Devuelve la vista que corresponde al componente.</summary>
        private ComponentControl GetComponent(BaseComponent component) => component switch {
            TransformComponent transform    => new TransformComponentView { DataContext = component },
            ScriptComponent script          => new ScriptComponentView { DataContext = component },
            _                               => throw new KeyNotFoundException()
        };
#endregion
    }
}
