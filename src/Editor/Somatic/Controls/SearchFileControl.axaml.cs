using Avalonia;
using Avalonia.Controls;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Controls.Model;
using Somatic.Services;
using Somatic.ViewModels;
using System.Collections.ObjectModel;

namespace Somatic.Controls {
    public partial class SearchFileControl : UserControl {
        private static readonly StyledProperty<FileItem?> SelectedItemProperty = AvaloniaProperty.Register<SearchFileControl, FileItem?>(nameof(SelectedItem));
        /// <summary>Texto que se utiliza de filtro en la búsqueda.</summary>
        public static readonly StyledProperty<string> SearchTextProperty = AvaloniaProperty.Register<SearchTreeControl, string>(nameof(SearchText), string.Empty);
        /// <summary>Texto que se utiliza como Watermark.</summary>
        public static readonly StyledProperty<string> SearchWatermarkProperty = AvaloniaProperty.Register<SearchTreeControl, string>(nameof(SearchWatermark), Framework.ServiceProvider?.GetRequiredService<ILocalizationService>().GetString("search") ?? "Buscar...");

        public ObservableCollection<FileItem> Items => [];
        /// <summary>Texto que se utiliza de filtro en la búsqueda.</summary>
        public string SearchText {
            get => GetValue(SearchTextProperty);
            set => SetValue(SearchTextProperty, value);
        }
        /// <summary>Texto que se utiliza como Watermark.</summary>
        public string SearchWatermark {
            get => GetValue(SearchWatermarkProperty);
            set => SetValue(SearchWatermarkProperty, value);
        }
        public FileItem? SelectedItem {
            get => GetValue(SelectedItemProperty);
            set => SetValue(SelectedItemProperty, value);
        }

        public SearchFileControl() {
            InitializeComponent();

            this.PropertyChanged += (s, e) => {
                if (e.Property == SelectedItemProperty) {

                }
            };
        }

        public FileExplorerViewModel? GetFileExplorerViewModel() => DataContext as FileExplorerViewModel;

    }
}
