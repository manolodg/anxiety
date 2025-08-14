using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Dock.Model.Core;
using Somatic.Model;
using Somatic.Services;
using System;
using System.Windows.Input;

namespace Somatic.ViewModels {
    /// <summary>Gestión de la pantalla principal de la aplicación.</summary>
    public partial class MainWindowViewModel : ViewModelBase {
#region Propiedades
        /// <summary>Raíz del dock.</summary>
        [ObservableProperty] private IRootDock _layout = null!;
#endregion

#region Campos
        private readonly ILocalizationService _localizationService = null!;
        private readonly IProjectService _projectService = null!;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public MainWindowViewModel() {
            if (Design.IsDesignMode) {
                IFactory factory = new MainDockFactory(Framework.ServiceProvider);
                Layout = factory.CreateLayout()!;
                factory.InitLayout(Layout);
            }
        }
        /// <summary>Crea una instancia de la clase.</summary>
        public MainWindowViewModel(
                IRootDock layout,
                ILocalizationService localizationService,
                IProjectService projectService) {
            Layout = layout;

            _localizationService = localizationService;
            _projectService = projectService;
        }
#endregion

#region Métodos
    #region Configuración
        /// <summary>Inicia el menú de la pantalla principal.</summary>
        public void InitializeMainMenu(Menu menu) {
            menu.Items.Add(new MenuItem { Header = "Guardar", Command = SaveProjectCommand });
        }
        /// <summary>Inicia el Toolbar.</summary>
        /// <param name="toolbar"></param>
        public void InitializeToolBar(StackPanel toolbar) {
            CreateButton(toolbar, "undo_icon", _localizationService.GetString("Main_Undo"));
            CreateButton(toolbar, "redo_icon", _localizationService.GetString("Main_Redo"));
        }
    #endregion

        /// <summary>Guarda el proyecto.</summary>
        [RelayCommand]
        private void SaveProject() => _projectService.SaveProject();

        /// <summary>Crea un botón a mostrar dentro del toolbar.</summary>
        private void CreateButton(StackPanel toolbar, string resourceKey, string tooltip, ICommand? command = null) {
            Button button = new Button { Width = 16, Height = 16, HorizontalContentAlignment = Avalonia.Layout.HorizontalAlignment.Center, VerticalContentAlignment = Avalonia.Layout.VerticalAlignment.Center };
            if (Application.Current!.Resources.TryGetResource(resourceKey, Application.Current!.ActualThemeVariant, out var resource) && resource is StreamGeometry geometry) {
                Path pathIcon = new Path {
                    Data = geometry,
                    Stretch = Stretch.UniformToFill
                };
                button.Content = pathIcon;
            }

            if (command != null) button.Command = command;

            ToolTip.SetTip(button, tooltip);

            toolbar.Children.Add(button);
        }
#endregion
    }
}
