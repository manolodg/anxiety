namespace Somatic.Core.History {
    /// <summary>Marca propiedades de estado del editor (p. ej. si un nodo está desplegado) que no deben entrar en el historial.</summary>
    [AttributeUsage(AttributeTargets.Property)]
    public sealed class UndoIgnoreAttribute : Attribute { }
}
