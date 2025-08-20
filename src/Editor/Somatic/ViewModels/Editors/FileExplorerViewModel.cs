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
    /// <summary>Realiza todas las operaciones relacionadas con las carpetas y archivos.</summary>
    public partial class FileExplorerViewModel : Tool {
#region Campos
        // Servicio para realizar el logging.
        private readonly ILogger<FileExplorerViewModel> _logger = null!;
        // Servicio para la traducción.
        private readonly ILocalizationService _localizationService = null!;
#endregion

#region Propiedades
        /// <summary>Ruta raíz para todos sus componentes.</summary>
        [ObservableProperty] private string _rootPath = string.Empty;
        /// <summary>Elementos contenidos en la carpeta.</summary>
        [ObservableProperty] private ObservableCollection<FileItem> _folderItems = [];

        /// <summary>Nodo seleccionado en el árbol de carpetas.</summary>
        [ObservableProperty] private FolderTreeNode? _selectedNode = null!;
        /// <summary>Elemento archivo seleccionado.</summary>
        [ObservableProperty] private FileItem? _selectedFile = null!;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public FileExplorerViewModel() {
            if (Design.IsDesignMode) {
                RootPath = "D:\\Anxiety";
            }
        }
        /// <summary>Crea una instancia de la clase.</summary>
        public FileExplorerViewModel(
                Project project, 
                ILogger<FileExplorerViewModel> logger, 
                ILocalizationService localizationService) {
            _logger = logger;
            _localizationService = localizationService;

            Title = _localizationService.GetString("FiEx_title");
            RootPath = project.Path;
        }
#endregion

#region Métodos
        public void LoadFolderContents() {
            if (SelectedNode == null) return;

            try {
                FolderItems.Clear();

                if (!Directory.Exists(SelectedNode.FullPath) || !IsWithinRootPath(SelectedNode.FullPath)) return;

                string[] directories = Directory.GetDirectories(SelectedNode.FullPath);
                foreach (string dir in directories) {
                    DirectoryInfo dirInfo = new DirectoryInfo(dir);
                    FolderItems.Add(new FileItem {
                        Name = dirInfo.Name,
                        FullPath = dir,
                        IsFile = false,
                        LastModified = dirInfo.LastWriteTime
                    });
                }

                string[] files = Directory.GetFiles(SelectedNode.FullPath);
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
#endregion
    }
}
