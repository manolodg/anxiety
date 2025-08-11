using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;
using Somatic.Model;
using Somatic.Services;
using System.Collections.ObjectModel;
using System.Linq;

namespace Somatic.ViewModels {
    /// <summary>Muestra y almacena los mensajes de Log.</summary>
    public partial class LoggerViewModel : Tool {
#region Campos
        /// <summary>Almacena todos los mensajes de log.</summary>
        private readonly ObservableCollection<LogMessage> _allMessages = [];

        /// <summary>Mensajes filtrados que se muestran en pantalla.</summary>
        [ObservableProperty] private ObservableCollection<LogMessage> _messages = [];

        /// <summary>Filtro que se utiliza para mostrar.</summary>
        private int _currentMask = (int)(MessageType.Information | MessageType.Warning | MessageType.Error);
#endregion

#region Constructor
        public LoggerViewModel() { }
        /// <summary>Constructor de este ViewModel.</summary>
        public LoggerViewModel(ILocalizationService localization) {
            this.Title = localization.GetString("Logg_title");
        }
#endregion

#region Métodos
        /// <summary>Añade un nuevo mensaje de log y aplica el filtrado..</summary>
        public void AddMessage(LogMessage message) {
            _allMessages.Add(message);
            Messages = new ObservableCollection<LogMessage>(_allMessages.Where(x => ((int)x.MessageType & _currentMask) != 0));
        }
        /// <summary>Indica la mascara y realiza el filtrado de mensajes.</summary>
        public void SetMask(int mask) {
            _currentMask = mask;
            Messages = new ObservableCollection<LogMessage>(_allMessages.Where(x => ((int)x.MessageType & _currentMask) != 0));
        }
        /// <summary>Limpia todos los mensajes.</summary>
        public void ClearMessages() {
            _allMessages.Clear();
            Messages = new ObservableCollection<LogMessage>(_allMessages.Where(x => ((int)x.MessageType & _currentMask) != 0));
        }
#endregion
    }
}
