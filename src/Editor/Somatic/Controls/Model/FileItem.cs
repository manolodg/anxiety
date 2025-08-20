using Avalonia.Media.Imaging;
using Avalonia.Platform;
using CommunityToolkit.Mvvm.ComponentModel;
using System;

namespace Somatic.Controls.Model {
    /// <summary>Elemento que configura visualmente el contenido de las carpetas.</summary>
    public partial class FileItem : ObservableObject {
#region Constantes
        // Ruta para localizar los iconos
        private const string DefaultPath = "avares://Somatic/Assets/Images/Icons/";
#endregion

#region Propiedades
        /// <summary>Indica si es una carpeta.</summary>
        public bool IsFolder => !IsFile;
        /// <summary>Indica la extensión del archivo.</summary>
        public string Extension => IsFile ? System.IO.Path.GetExtension(Name) : string.Empty;
        /// <summary>Indica el nombre del archivo sin la extensión.</summary>
        public string FileNameWithoutExtension => IsFile ? System.IO.Path.GetFileNameWithoutExtension(Name) : Name;

        /// <summary>Nombre.</summary>
        [ObservableProperty] private string _name = string.Empty;
        /// <summary>Ruta completa.</summary>
        [ObservableProperty] private string _fullPath = string.Empty;
        /// <summary>Indica si es un archivo.</summary>
        [ObservableProperty] private bool _isFile;
        /// <summary>Tamaño del archivo.</summary>
        [ObservableProperty] private string _size = string.Empty;
        /// <summary>Fecha última modificación.</summary>
        [ObservableProperty] private DateTime _lastModified;
        /// <summary>Indica si está seleccionado.</summary>
        [ObservableProperty] private bool _isSelected;
#endregion

#region Métodos
        /// <summary>Icono del archivo.</summary>
        public Bitmap Icon => new Bitmap(AssetLoader.Open(new Uri(GetIcon())));

        /// <summary>Configuración del icono.</summary>
        private string GetIcon() {
            if (IsFolder) return $"{DefaultPath}Folder.png";

            return Extension switch {
                ".scene"    => $"{DefaultPath}SnowBall.png",
                ".soma"     => $"{DefaultPath}FileChart.png",
                _           => $"{DefaultPath}Document.png"
            };
        }
#endregion
    }
}
