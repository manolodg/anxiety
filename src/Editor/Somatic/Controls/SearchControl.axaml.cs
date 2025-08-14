using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Services;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Somatic.Controls {
    /// <summary>Control para realizar la búsqueda dentro de un listado que le proporcionamos.</summary>
    public partial class SearchControl : UserControl {
#region Propiedades Registradas
        /// <summary>Elementos filtrados.</summary>
        public static readonly DirectProperty<SearchControl, ObservableCollection<object?>> FilteredItemsProperty = AvaloniaProperty.RegisterDirect<SearchControl, ObservableCollection<object?>>(nameof(FilteredItems), o => o.FilteredItems!);

        /// <summary>Fuente de elementos.</summary>
        public static readonly StyledProperty<IEnumerable?> ItemsSourceProperty = AvaloniaProperty.Register<SearchControl, IEnumerable?>(nameof(ItemsSource));
        /// <summary>Texto que se utiliza de filtro en la búsqueda.</summary>
        public static readonly StyledProperty<string> SearchTextProperty = AvaloniaProperty.Register<SearchControl, string>(nameof(SearchText), string.Empty);
        /// <summary>Watermark.</summary>
        public static readonly StyledProperty<string> SearchWatermarkProperty = AvaloniaProperty.Register<SearchControl, string>(nameof(SearchWatermark), Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("search") ?? "Buscar...");
        /// <summary>Elemento seleccionado en el ListBox.</summary>
        public static readonly StyledProperty<object?> SelectedItemProperty = AvaloniaProperty.Register<SearchControl, object?>(nameof(SelectedItem));
        /// <summary>Formato del ListBoxItem.</summary>
        public static readonly StyledProperty<IDataTemplate?> ItemTemplateProperty = AvaloniaProperty.Register<SearchControl, IDataTemplate?>(nameof(ItemTemplate));
        /// <summary>Función que se utiliza para la búsqueda.</summary>
        public static readonly StyledProperty<Func<object, string, bool>> FilterFunctionProperty = AvaloniaProperty.Register<SearchControl, Func<object, string, bool>>(nameof(FilterFunction));
#endregion

#region Eventos
        /// <summary>Se lanza el evento cuando cambia la selección.</summary>
        public event EventHandler<SelectionChangedEventArgs> OnSelectionChanged;
#endregion

#region Propiedades
        public ObservableCollection<object>? FilteredItems {
            get => _filteredItems;
            private set => SetAndRaise(FilteredItemsProperty!, ref _filteredItems, value);
        }

        public IEnumerable? ItemsSource {
            get => GetValue(ItemsSourceProperty);
            set => SetValue(ItemsSourceProperty, value);
        }
        public string SearchText {
            get => GetValue(SearchTextProperty);
            set => SetValue(SearchTextProperty, value);
        }
        public string SearchWatermark {
            get => GetValue(SearchWatermarkProperty);
            set => SetValue(SearchWatermarkProperty, value);
        }
        public object? SelectedItem {
            get => GetValue(SelectedItemProperty);
            set => SetValue(SelectedItemProperty, value);
        }
        public IDataTemplate? ItemTemplate {
            get => GetValue(ItemTemplateProperty);
            set => SetValue(ItemTemplateProperty, value);
        }
        public Func<object, string, bool>? FilterFunction {
            get => GetValue(FilterFunctionProperty);
            set => SetValue(FilterFunctionProperty!, value);
        }
#endregion

#region Campos
        private readonly List<object> _allItems = [];
        
        private ObservableCollection<object>? _filteredItems;
#endregion

#region Constructores
#pragma warning disable CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
        public SearchControl() {
            InitializeComponent();

            _filteredItems = [];

            SetDefaultTemplate();
        }
#pragma warning restore CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
#endregion

#region Métodos
        /// <summary>Se realiza el filtrado según cambién las propiedades.</summary>
        /// <param name="change"></param>
        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);

            if (change.Property == ItemsSourceProperty) {
                OnItemsSourceChanged(change.NewValue as IEnumerable);
            } else if (change.Property == SelectedItemProperty) {
                OnSelectionChanged?.Invoke(this, new SelectionChangedEventArgs(null!, null!, new List<object?> { change.NewValue }));
            } else if (change.Property == SearchTextProperty) {
                OnSearchTextChanged(change.NewValue as string ?? string.Empty);
            } else if (change.Property == ItemTemplateProperty && change.NewValue == null) {
                SetDefaultTemplate();
            }
        }

        private void SetDefaultTemplate() {
            ItemTemplate ??= new FuncDataTemplate<object>((value, namescope) =>
                new TextBlock {
                    Text = value?.ToString() ?? string.Empty,
                    Padding = new Thickness(8, 4),
                    VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center
                });
        }
        private void OnItemsSourceChanged(IEnumerable? newItems) {
            _allItems.Clear();

            if (newItems != null) {
                foreach (var item in newItems) {
                    _allItems.Add(item);
                }
            }

            FilterItems();
        }

        private void OnSearchTextChanged(string searchText) {
            FilterItems();
        }

        private void FilterItems() {
            if (_filteredItems == null) return;

            _filteredItems.Clear();

            var searchText = SearchText?.Trim() ?? string.Empty;

            IEnumerable<object> filtered;

            if (string.IsNullOrWhiteSpace(searchText)) {
                filtered = _allItems;
            } else {
                // Usar función de filtrado personalizada si est� definida
                if (FilterFunction != null) {
                    filtered = _allItems.Where(item => FilterFunction(item, searchText));
                } else {
                    // Función de filtrado por defecto
                    filtered = _allItems.Where(item =>
                        item?.ToString()?.ToLowerInvariant().Contains(searchText.ToLowerInvariant()) == true);
                }
            }

            ObservableCollection<object> newFilteredItems = new ObservableCollection<object>(filtered);
            SetAndRaise(FilteredItemsProperty!, ref _filteredItems, newFilteredItems);
        }
#endregion
    }
}
