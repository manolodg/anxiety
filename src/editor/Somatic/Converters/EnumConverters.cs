using Avalonia.Data.Converters;
using Somatic.Localization;
using System.Globalization;

namespace Somatic.Converters {
    /// <summary>Valor de un enum a su texto localizado; la clave de localización es el nombre del valor.</summary>
    public sealed class LocalizedEnumConverter : IValueConverter {
        public static readonly LocalizedEnumConverter Instance = new();

        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) => value is Enum e ? LocalizeExtension.Get(e.ToString()) : value;

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => throw new NotSupportedException();
    }

    /// <summary>
    /// <c>true</c> si el valor del enum está en la lista del parámetro (nombres separados por comas). Sirve para
    /// mostrar propiedades que sólo tienen sentido en algunos modos (p. ej. el ángulo sólo en luces de foco).
    /// </summary>
    public sealed class EnumMatchConverter : IValueConverter {
        public static readonly EnumMatchConverter Instance = new();

        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) {
            if (value is not Enum e || parameter is not string names) return false;

            string name = e.ToString();
            foreach (string candidate in names.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries)) {
                if (candidate == name) return true;
            }
            return false;
        }

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => throw new NotSupportedException();
    }
}
