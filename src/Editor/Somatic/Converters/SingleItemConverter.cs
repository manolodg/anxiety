using Avalonia.Data.Converters;
using System;
using System.Collections;
using System.Globalization;

namespace Somatic.Converters {
    /// <summary>Convierte un único objeto a una colección.</summary>
    public class SingleItemConverter : IValueConverter {
#region Métodos
        /// <summary>Realiza la conversión de un objeto a un array.</summary>
        /// <param name="value">Valor del objeto a convertir.</param>
        /// <param name="targetType">Tipo de valor objetivo.</param>
        /// <param name="parameter">Parámetros para la conversión.</param>
        /// <param name="culture">Language usado para la conversión.</param>
        /// <returns>Resultado de la operación.</returns>
        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) {
            if (value == null) return null;
            return new[] { value };
        }

        /// <summary>Realiza la conversión de un array a un objeto.</summary>
        /// <param name="value">Valor del objeto a convertir.</param>
        /// <param name="targetType">Tipo de valor objetivo.</param>
        /// <param name="parameter">Parámetros para la conversión.</param>
        /// <param name="culture">Language usado para la conversión.</param>
        /// <returns>Resultado de la operación.</returns>
        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) {
            if (value is IEnumerable enumerable) {
                IEnumerator enumerator = enumerable.GetEnumerator();
                return enumerator.MoveNext() ? enumerator.Current : null;
            }

            return value;
        }
#endregion
    }
}
