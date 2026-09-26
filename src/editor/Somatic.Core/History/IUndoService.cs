using System.ComponentModel;

namespace Somatic.Core.History {
    /// <summary>Historial de deshacer/rehacer del editor.</summary>
    public interface IUndoService : INotifyPropertyChanged {
        bool CanUndo { get; }
        bool CanRedo { get; }

        string? UndoDescription { get; }
        string? RedoDescription { get; }

        /// <summary><c>true</c> mientras se ejecuta, deshace o rehace un comando: los cambios que provoca no deben registrarse otra vez.</summary>
        bool IsApplying { get; }

        /// <summary>Ejecuta el comando y lo apila.</summary>
        void Execute(IUndoableCommand command);

        /// <summary>Apila un comando cuyo efecto ya se ha producido (p. ej. una propiedad editada desde un binding).</summary>
        void Record(IUndoableCommand command);

        void Undo();
        void Redo();
        void Clear();
    }
}
