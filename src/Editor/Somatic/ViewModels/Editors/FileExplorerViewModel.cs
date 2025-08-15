using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Microsoft.Extensions.Logging;
using Somatic.Model;
using Somatic.Services;

namespace Somatic.ViewModels {
    public partial class FileExplorerViewModel : Tool {
        private readonly ILogger<FileExplorerViewModel> _logger;
        private readonly ILocalizationService _localizationService;

        [ObservableProperty] private string _rootPath = string.Empty;

        public FileExplorerViewModel() {
            if (Design.IsDesignMode) {
                RootPath = "D:\\Anxiety";
            }
        }
        public FileExplorerViewModel(
                Project project, 
                ILogger<FileExplorerViewModel> logger, 
                ILocalizationService localizationService) {
            _logger = logger;
            _localizationService = localizationService;

            Title = localizationService.GetString("FiEx_title");
            RootPath = project.Path;
        }
    }
}
