using Dock.Model.Mvvm.Controls;
using Somatic.Model;
using Somatic.Services;
using System.Collections.ObjectModel;

namespace Somatic.ViewModels {
    public partial class ClipboardViewModel : Tool {
        private readonly IUndoRedoService _undoRedoService;

        public ReadOnlyObservableCollection<UndoRedoAction> UndoList { get; }
        public ReadOnlyObservableCollection<UndoRedoAction> RedoList { get; }

        public ClipboardViewModel(ILocalizationService localization, IUndoRedoService undoredoservice) {
            this.Title = localization.GetString("Clip_title");

            _undoRedoService = undoredoservice;

            UndoList = new ReadOnlyObservableCollection<UndoRedoAction>(_undoRedoService.UndoList);
            RedoList = new ReadOnlyObservableCollection<UndoRedoAction>(_undoRedoService.RedoList);
        }
    }
}
