using Avalonia;
using Avalonia.VisualTree;
using System.Collections.Generic;

namespace Somatic.Extensions {
    /// <summary>Extensiones para las busquedas de elementos visuales.</summary>
    public static class VisualExtension {
#region Métodos
        /// <summary>Localiza todos los elementos hijo de un tipo concreto.</summary>
        /// <typeparam name="TOutput">Tipo que se realiza la búsqueda.</typeparam>
        /// <param name="input">Elemento de raíz de la búsqueda.</param>
        /// <returns>Los elementos encontrados.</returns>
        public static List<TOutput> FindChilds<TOutput>(this Visual input) {
            List<TOutput> items = [];
            FindItems(input);
            return items;

            void FindItems(Visual visual) {
                if (visual is TOutput output) items.Add(output);

                foreach (Visual child in visual.GetVisualChildren()) {
                    FindItems(child);
                }
            }
        }
        /// <summary>Localiza el elemento padre del tipo indicado.</summary>
        /// <typeparam name="TOutput">Tipo indicado de padre.</typeparam>
        /// <param name="input">Elemento raíz de búsqueda.</param>
        /// <returns>Elemento encontrado</returns>
        public static TOutput? FindParent<TOutput>(this Visual input) {
            StyledElement? current = input.Parent;
            while (current != null) {
                if (current is TOutput ic) return ic;
                current = (current as Visual)?.GetVisualParent();
            }
            return default;
        }
#endregion
    }
}
