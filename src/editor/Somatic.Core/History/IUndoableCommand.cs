namespace Somatic.Core.History {
    /// <summary>Operación reversible del editor. <see cref="Execute"/> se llama también al rehacer.</summary>
    public interface IUndoableCommand {
        /// <summary>Texto corto para el menú ("Mover «Luz»").</summary>
        string Description { get; }

        void Execute();
        void Undo();
    }

    /// <summary>Comando que puede absorber al siguiente (p. ej. varias pulsaciones al teclear un nombre = un solo paso).</summary>
    public interface IMergeableCommand : IUndoableCommand {
        /// <summary>Si devuelve <c>true</c>, <paramref name="next"/> queda incorporado a este comando y se descarta.</summary>
        bool TryMerge(IUndoableCommand next);
    }
}
