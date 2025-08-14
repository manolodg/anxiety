using Avalonia.Controls;
using Microsoft.Extensions.DependencyInjection;
using MsBox.Avalonia;
using MsBox.Avalonia.Base;
using MsBox.Avalonia.Enums;
using Somatic.Services;
using Somatic.ViewModels;

namespace Somatic.Views {
    /// <summary>Control de la pantalla principal.</summary>
    public partial class MainWindow : Window {
#region Métodos
        /// <summary>Crea una instancia de la clase.</summary>
        public MainWindow() => InitializeComponent();
        /// <summary>Crea una instancia de la clase.</summary>
        public MainWindow(IConfigurationService configuration) {
            InitializeComponent();


            this.Loaded += async (s, e) => {
                ((MainWindowViewModel)DataContext!).InitializeMainMenu(MainMenu);
                ((MainWindowViewModel)DataContext!).InitializeToolBar(ToolBar);

                if (!configuration.LoadWindowState(this)) {
                    var localizacion = Framework.ServiceProvider.GetRequiredService<ILocalizationService>();
                    IMsBox<ButtonResult> dlg = MessageBoxManager.GetMessageBoxStandard("Error", localizacion.GetString("Msg_notconfig"), ButtonEnum.Ok, MsBox.Avalonia.Enums.Icon.Error);
                    await dlg.ShowAsync();

                    this.Close();
                }
            };
            this.Closing += (s, e) => configuration.SaveWindowState(this);

        }
#endregion
    }
}