using Avalonia.Media.Imaging;
using Avalonia.Platform;
using CommunityToolkit.Mvvm.ComponentModel;
using System;

namespace Somatic.Controls.Model {
    public partial class FileItem : ObservableObject {
        private const string DefaultPath = "avares://Somatic/Assets/Images/Icons/";

        public bool IsFolder => !IsFile;
        public string Extension => IsFile ? System.IO.Path.GetExtension(Name) : string.Empty;
        public string FileNameWithoutExtension => IsFile ? System.IO.Path.GetFileNameWithoutExtension(Name) : Name;

        [ObservableProperty] private string _name = string.Empty;
        [ObservableProperty] private string _fullPath = string.Empty;
        [ObservableProperty] private bool _isFile;
        [ObservableProperty] private string _size = string.Empty;
        [ObservableProperty] private DateTime _lastModified;
        [ObservableProperty] private bool _isSelected;

        public Bitmap Icon => new Bitmap(AssetLoader.Open(new Uri(GetIcon())));

        private string GetIcon() {
            if (IsFolder) return $"{DefaultPath}Folder.png";

            return Extension switch {
                ".scene" => $"{DefaultPath}SnowBall.png",
                ".soma" => $"{DefaultPath}FileChart.png",
                _ => $"{DefaultPath}Document.png"
            };
        }
    }
}
