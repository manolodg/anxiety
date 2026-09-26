using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.Logging;
using Somatic.Core.History;

namespace Somatic.Infrastructure.History {
    public sealed class UndoService : ObservableObject, IUndoService {
        /// <summary>Pasos que se conservan; los más antiguos se descartan.</summary>
        internal const int MaxHistory = 500;

        private readonly LinkedList<IUndoableCommand> _undo = new();
        private readonly Stack<IUndoableCommand>      _redo = new();
        private readonly ILogger<UndoService> _logger;

        // Tras deshacer/rehacer, el siguiente cambio nunca se fusiona con lo que quede en la cima.
        private bool _mergeBarrier;

        public UndoService(ILogger<UndoService> logger) {
            _logger = logger;
        }

        public bool CanUndo => _undo.Count > 0;
        public bool CanRedo => _redo.Count > 0;

        public string? UndoDescription => _undo.Last?.Value.Description;
        public string? RedoDescription => _redo.Count > 0 ? _redo.Peek().Description : null;

        public bool IsApplying { get; private set; }

        public void Execute(IUndoableCommand command) {
            Apply(command.Execute);
            Record(command);
        }

        public void Record(IUndoableCommand command) {
            _redo.Clear();

            if (!_mergeBarrier && _undo.Last?.Value is IMergeableCommand top && top.TryMerge(command)) {
                NotifyChanged();
                return;
            }

            _mergeBarrier = false;
            _undo.AddLast(command);
            if (_undo.Count > MaxHistory) _undo.RemoveFirst();
            NotifyChanged();
        }

        public void Undo() {
            if (_undo.Last is null) return;

            IUndoableCommand command = _undo.Last.Value;
            _undo.RemoveLast();
            Apply(command.Undo);
            _redo.Push(command);

            _mergeBarrier = true;
            _logger.LogDebug("Deshacer: {Description}", command.Description);
            NotifyChanged();
        }

        public void Redo() {
            if (_redo.Count == 0) return;

            IUndoableCommand command = _redo.Pop();
            Apply(command.Execute);
            _undo.AddLast(command);

            _mergeBarrier = true;
            _logger.LogDebug("Rehacer: {Description}", command.Description);
            NotifyChanged();
        }

        public void Clear() {
            _undo.Clear();
            _redo.Clear();
            _mergeBarrier = false;
            NotifyChanged();
        }

        private void Apply(Action action) {
            IsApplying = true;
            try {
                action();
            } finally {
                IsApplying = false;
            }
        }

        private void NotifyChanged() {
            OnPropertyChanged(nameof(CanUndo));
            OnPropertyChanged(nameof(CanRedo));
            OnPropertyChanged(nameof(UndoDescription));
            OnPropertyChanged(nameof(RedoDescription));
        }
    }
}
