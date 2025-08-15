using System;

namespace Somatic.Model {
    public class UndoRedoAction {
        public string Name { get; }

        private readonly Action _undoAction;
        private readonly Action _redoAction;

        public UndoRedoAction(Action undo, Action redo, string name) {
            _undoAction = undo;
            _redoAction = redo;

            Name = name;
        }

        public void Redo() => _redoAction();
        public void Undo() => _undoAction();
    }
}
