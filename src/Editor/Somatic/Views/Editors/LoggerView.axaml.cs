using Avalonia.Controls;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Somatic.Model;
using Somatic.ViewModels;

namespace Somatic.Views {
    /// <summary>Controles de visualización del Logger.</summary>
    public partial class LoggerView : UserControl {
#region Constructor
        /// <summary>Crea una instancia de la clase <see cref="LoggerView"/></summary>
        public LoggerView() {
            InitializeComponent();

            this.SizeChanged += (s, e) => { ConsoleScrollViewer.Height = e.NewSize.Height; };
            this.Loaded += (s, e) => {
                ILogger<LoggerView> logger = Framework.ServiceProvider.GetRequiredService<ILogger<LoggerView>>();

                logger.LogInformation("prueba de log de información.");
                logger.LogWarning("prueba de log de warning.");
                logger.LogError("prueba de log de error.");
            };
        }
#endregion

#region Métodos
        /// <summary>Asigna el foco a este control cuando pasa el ratón sobre él.</summary>
        private void OnDockControlPointerEntered(object? sender, Avalonia.Input.PointerEventArgs e) {
            if (sender is Control control && !control.IsFocused) control.Focus();
        }
        /// <summary>Ejecuta la limpieza de mensajes cuando se pulsa el botón "Limpiar"</summary>
        private void Button_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
            (DataContext as LoggerViewModel)!.ClearMessages();
        }
        /// <summary>Cambia y aplica el filtro cuando lo haga el usuario.</summary>
        private void ToggleButton_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
            int mask = (toggleInfo.IsChecked  == true ? (int)MessageType.Information : 0) |
                       (toggleWarn.IsChecked  == true ? (int)MessageType.Warning : 0) |
                       (toggleError.IsChecked == true ? (int)MessageType.Error : 0);

            (DataContext as LoggerViewModel)!.SetMask(mask);
        }
#endregion
    }
}

