using Somatic.Model;
using System.Collections.ObjectModel;
using System.Linq;

namespace Somatic.Services {
    public class UndoRedoService : IUndoRedoService {
        private readonly ObservableCollection<UndoRedoAction> _redolist = [];
        private readonly ObservableCollection<UndoRedoAction> _undolist = [];

        public ObservableCollection<UndoRedoAction> RedoList => _redolist;
        public ObservableCollection<UndoRedoAction> UndoList => _undolist;

        private bool _addEnabled = true;

        public void Reset() {
            _redolist.Clear();
            _undolist.Clear();
        }

        public void AddAction(UndoRedoAction command) {
            if (_addEnabled) {
                _undolist.Add(command);
                _redolist.Clear();

                command.Redo();
            }
        }

        public void Undo() {
            if (_undolist.Any()) {
                UndoRedoAction command = _undolist.Last();
                _undolist.RemoveAt(_undolist.Count - 1);

                _addEnabled = false;
                command.Undo();
                _addEnabled = true;

                _redolist.Insert(0, command);
            }
        }
        public void Redo() {
            if (_redolist.Any()) {
                UndoRedoAction command = _redolist.First();
                _redolist.RemoveAt(0);

                _addEnabled = false;
                command.Redo();
                _addEnabled = true;

                _undolist.Add(command);
            }
        }
    }
}
