using Somatic.Model;
using System.Collections.ObjectModel;

namespace Somatic.Services {
    public interface IUndoRedoService {
        ObservableCollection<UndoRedoAction> RedoList { get; }
        ObservableCollection<UndoRedoAction> UndoList { get; }

        void Reset();
        void AddAction(UndoRedoAction command);

        void Undo();
        void Redo();
    }
}
