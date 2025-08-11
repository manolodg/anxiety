using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Services;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;

namespace Somatic.Controls {
    /// <summary></summary>
    public partial class FolderPickerControl : UserControl, INotifyDataErrorInfo {
#region Propiedades Avalonia
        /// <summary>Ruta que está seleccionada.</summary>
        public static readonly StyledProperty<string> SelectedPathProperty = AvaloniaProperty.Register<FolderPickerControl, string>(nameof(SelectedPath), string.Empty);
        /// <summary>Rutas que ya existen en el sistema.</summary>
        public static readonly StyledProperty<IEnumerable<string>> AvailablePathsProperty = AvaloniaProperty.Register<FolderPickerControl, IEnumerable<string>>(nameof(AvailablePaths), new ObservableCollection<string>());
        /// <summary>Marca de agua del control.</summary>
        public static readonly StyledProperty<string> WatermarkProperty = AvaloniaProperty.Register<FolderPickerControl, string>(nameof(Watermark), Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("FoldCtrl_choose"));
        /// <summary>Indica si está desplegado o no el Dropdown.</summary>
        public static readonly StyledProperty<bool> IsDropdownOpenProperty = AvaloniaProperty.Register<FolderPickerControl, bool>(nameof(IsDropdownOpen), false);
#endregion

#region Propiedades
        /// <summary>Ruta que está seleccionada.</summary>
        public string SelectedPath {
            get => GetValue(SelectedPathProperty);
            set => SetValue(SelectedPathProperty, value);
        }
        /// <summary>Rutas que ya existen en el sistema.</summary>
        public IEnumerable<string> AvailablePaths {
            get => GetValue(AvailablePathsProperty);
            set => SetValue(AvailablePathsProperty, value);
        }
        /// <summary>Marca de agua del control.</summary>
        public string Watermark {
            get => GetValue(WatermarkProperty);
            set => SetValue(WatermarkProperty, value);
        }
        /// <summary>Indica si está desplegado o no el Dropdown.</summary>
        public bool IsDropdownOpen {
            get => GetValue(IsDropdownOpenProperty);
            set => SetValue(IsDropdownOpenProperty, value);
        }

        public bool HasErrors => true;
#endregion

#region Eventos
        /// <summary>Indica que se ha cambiado la ruta seleccionada.</summary>
        public event EventHandler<string>? PathSelected;
        /// <summary>Indica que se ha cambiado el estado de los errores.</summary>
        public event EventHandler<DataErrorsChangedEventArgs>? ErrorsChanged;
#endregion

#region Campos
        // Campos una vez filtrados
        private ObservableCollection<string> _filteredPaths = [];
#endregion

#region Constructor
        /// <summary>Crea una instancia de la clase <see cref="FolderPickerControl"/>.</summary>
        public FolderPickerControl() {
            InitializeComponent();
            UpdateFilteredPaths();
        }
#endregion

#region Métodos
        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            base.OnPropertyChanged(change);

            if (change.Property == AvailablePathsProperty) {
                UpdateFilteredPaths();
            } else if (change.Property == IsDropdownOpenProperty && PART_Popup != null) {
                PART_Popup.IsOpen = IsDropdownOpen;
            }
        }

        /// <summary>Se ha cambiado el texto del TextBox.</summary>
        private void OnTextBoxTextChanged(object? sender, TextChangedEventArgs e) {
            UpdateFilteredPaths();

            IsDropdownOpen = !string.IsNullOrEmpty(SelectedPath) && _filteredPaths.Count > 0;
        }
        /// <summary>Control de teclas especiales.</summary>
        private void OnTextBoxKeyDown(object? sender, KeyEventArgs e) {
            if (e.Key == Key.Down && _filteredPaths.Count > 0) {
                IsDropdownOpen = true;
                PART_SuggestionsList?.Focus();
                if (PART_SuggestionsList?.SelectedIndex == -1) PART_SuggestionsList.SelectedIndex = 0;
            } else if (e.Key == Key.Escape) {
                IsDropdownOpen = false;
            } else if (e.Key == Key.Enter) {
                IsDropdownOpen = false;
                PathSelected?.Invoke(this, SelectedPath);
            }
        }
        /// <summary>Se ha perdido el foco.</summary>
        private void OnTextBoxLostFocus(object? sender, RoutedEventArgs e) {
            // Retrasar el cierre para permitir la selección en la lista
            Dispatcher.UIThread.Post(() => { if (!PART_SuggestionsList?.IsPointerOver == true) IsDropdownOpen = false; }, DispatcherPriority.Background);
        }

        /// <summary>Pulsado el botón del explorador.</summary>
        private async void OnBrowseButtonClick(object? sender, RoutedEventArgs e) {
            TopLevel? topLevel = TopLevel.GetTopLevel(this);
            if (topLevel != null) {
                FolderPickerOpenOptions folderOptions = new FolderPickerOpenOptions {
                    Title = Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("FoldCtrl_choose"),
                    AllowMultiple = true,
                };

                // Si hay una ruta seleccionada, usarla como punto de partida
                if (!string.IsNullOrEmpty(SelectedPath) && System.IO.Directory.Exists(SelectedPath)) {
                    try {
                        folderOptions.SuggestedStartLocation = await topLevel.StorageProvider.TryGetFolderFromPathAsync(SelectedPath);
                    } catch {
                        // Ignorar errores al acceder a la carpeta sugerida
                    }
                }

                IReadOnlyList<IStorageFolder> result = await topLevel.StorageProvider.OpenFolderPickerAsync(folderOptions);
                if (result.Count > 0) {
                    IStorageFolder selectedFolder = result[0];
                    SelectedPath = selectedFolder.Path.LocalPath;

                    PathSelected?.Invoke(this, SelectedPath);
                }
            }
        }

        /// <summary>Cuando se cambia la selección de las sugerencias.</summary>
        private void OnSuggestionsListSelectionChanged(object? sender, SelectionChangedEventArgs e) {
            if (PART_SuggestionsList?.SelectedItem is string selectedPath) SelectedPath = selectedPath;
        }
        /// <summary>Al hacer doble click en la sugerencia.</summary>
        private void OnSuggestionsListDoubleTapped(object? sender, TappedEventArgs e) {
            if (PART_SuggestionsList?.SelectedItem is string selectedPath) {
                SelectedPath = selectedPath;

                IsDropdownOpen = false;
                PathSelected?.Invoke(this, SelectedPath);
            }
        }

        /// <summary>Ajustamos el ancho al control</summary>
        private void OnPopupOpened(object? sender, EventArgs e) => PART_Popup.Width = Bounds.Width;
        /// <summary>Borramos la selección al cerrar.</summary>
        private void OnPopupClosed(object? sender, EventArgs e) {
            if (PART_SuggestionsList != null) PART_SuggestionsList.SelectedIndex = -1;
        }

        /// <summary>Filtramos los Paths que tenemos según el texto introducido de búsqueda.</summary>
        private void UpdateFilteredPaths() {
            _filteredPaths.Clear();

            ErrorsChanged?.Invoke(this, new DataErrorsChangedEventArgs(nameof(SelectedPath)));

            if (AvailablePaths != null) {
                string filterText = SelectedPath.ToLowerInvariant() ?? string.Empty;
                string[] matches = AvailablePaths
                    .Where(path => string.IsNullOrEmpty(filterText) || (path.ToLowerInvariant().Contains(filterText) && path.ToLowerInvariant() != filterText))
                    .Take(10)
                    .ToArray();

                foreach (string match in matches) {
                    _filteredPaths.Add(match);
                }
            }

            if (PART_SuggestionsList != null) PART_SuggestionsList.ItemsSource = _filteredPaths;
        }


        public IEnumerable GetErrors(string? propertyName) {
            return null;
        }
#endregion
    }
}
