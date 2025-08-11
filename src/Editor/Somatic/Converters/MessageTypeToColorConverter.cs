using Avalonia.Data.Converters;
using Avalonia.Media;
using Somatic.Model;
using System;
using System.Globalization;

namespace Somatic.Converters {
    /// <summary>Realiza la interpretación del color para el filtro de Log.</summary>
    public class MessageTypeToColorConverter : IValueConverter {
#region Métodos
        /// <summary>Obtiene el color según el tipo de log.</summary>
        /// <param name="value">Valor del objeto a convertir.</param>
        /// <param name="targetType">Tipo de valor objetivo.</param>
        /// <param name="parameter">Parámetros para la conversión.</param>
        /// <param name="culture">Language usado para la conversión.</param>
        /// <returns>Resultado de la operación.</returns>
        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) {
            if (value is MessageType messageType) {
                return messageType switch {
                    MessageType.Warning => new SolidColorBrush(0xffffbb22),
                    MessageType.Error   => new SolidColorBrush(0xffff4456),
                    _                   => Brushes.Black
                };
            }

            return Brushes.Black;
        }

        /// <summary>No aplica.</summary>
        /// <param name="value">Valor del objeto a convertir.</param>
        /// <param name="targetType">Tipo de valor objetivo.</param>
        /// <param name="parameter">Parámetros para la conversión.</param>
        /// <param name="culture">Language usado para la conversión.</param>
        /// <returns>Resultado de la operación.</returns>
        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) {
            throw new NotImplementedException();
        }
#endregion
    }
}
