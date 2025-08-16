using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Microsoft.Extensions.Logging;
using Somatic.Controls.Model;
using Somatic.Model;
using Somatic.Services;
using System;
using System.Collections.ObjectModel;
using System.IO;

namespace Somatic.ViewModels {
    public partial class FileExplorerViewModel : Tool {
        private readonly ILogger<FileExplorerViewModel> _logger;
        private readonly ILocalizationService _localizationService;

        [ObservableProperty] private string _rootPath = string.Empty;
        [ObservableProperty] private string? _selectedPath = string.Empty;
        [ObservableProperty] private ObservableCollection<FileItem> _folderItems = [];
        [ObservableProperty] private FileItem? _selectedFile;

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
            SelectedPath = RootPath = project.Path;
        }

        public void LoadFolderContents() {
            try {
                FolderItems.Clear();

                if (!Directory.Exists(SelectedPath) || !IsWithinRootPath(SelectedPath)) return;

                string[] directories = Directory.GetDirectories(SelectedPath);
                foreach (string dir in directories) {
                    DirectoryInfo dirInfo = new DirectoryInfo(dir);
                    FolderItems.Add(new FileItem {
                        Name = dirInfo.Name,
                        FullPath = dir,
                        IsFile = false,
                        LastModified = dirInfo.LastWriteTime
                    });
                }

                string[] files = Directory.GetFiles(SelectedPath);
                foreach (string file in files) {
                    FileInfo fileInfo = new FileInfo(file);
                    FolderItems.Add(new FileItem {
                        Name = fileInfo.Name,
                        FullPath = file,
                        IsFile = true,
                        Size = FormatFileSize(fileInfo.Length),
                        LastModified = fileInfo.LastWriteTime
                    });
                }
            } catch (Exception ex) {
                _logger.LogError($"Error cargando el contenido de las carpetas: {ex.Message}");
            }
        }
        private bool IsWithinRootPath(string path) {
            if (string.IsNullOrEmpty(RootPath) || string.IsNullOrEmpty(path)) return false;

            string rootFullPath = Path.GetFullPath(RootPath);
            string checkFullPath = Path.GetFullPath(path);

            return checkFullPath.StartsWith(rootFullPath, StringComparison.OrdinalIgnoreCase);
        }
        private string FormatFileSize(long bytes) {
            string[] sizes = { "B", "KB", "MB", "GB", "TB" };
            double len = bytes;

            int order = 0;
            while (len > 1024 && order < sizes.Length - 1) {
                order++;
                len = len / 1024;
            }

            return $"{len:0.##} {sizes[order]}";
        }
    }
}
