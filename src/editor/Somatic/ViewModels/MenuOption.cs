using System.Windows.Input;

namespace Somatic.ViewModels {
    /// <summary>Entrada de un menú generado desde el ViewModel (se pinta con el tema <c>MenuOptionItemTheme</c>).</summary>
    public sealed record MenuOption(string Title, string IconKey, ICommand Command, object? CommandParameter = null);
}
