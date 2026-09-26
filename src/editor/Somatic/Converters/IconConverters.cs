using Avalonia;
using Avalonia.Data.Converters;
using Avalonia.Media;
using Somatic.Core.Scene;
using System.Globalization;

namespace Somatic.Converters {
    /// <summary>Clave de recurso (p. ej. <c>"IconCamera"</c>) a la geometría del icono definida en el tema.</summary>
    public sealed class IconResourceConverter : IValueConverter {
        public static readonly IconResourceConverter Instance = new();

        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) => value is string key ? Find(key) : null;

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => throw new NotSupportedException();

        internal static Geometry? Find(string key) {
            Application? application = Application.Current;
            if (application is null) return null;

            return application.TryGetResource(key, application.ActualThemeVariant, out object? resource) ? resource as Geometry : null;
        }
    }

    /// <summary>Tipo de entidad a su icono en la jerarquía: grupo = caja, cámara = cámara, luz = foco...</summary>
    public sealed class EntityIconConverter : IValueConverter {
        public static readonly EntityIconConverter Instance = new();

        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture) => value is EntityKind kind ? IconResourceConverter.Find(IconKeyFor(kind)) : null;

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => throw new NotSupportedException();

        public static string IconKeyFor(EntityKind kind) => kind switch {
            EntityKind.Group  => "IconBox",
            EntityKind.Camera => "IconCamera",
            EntityKind.Light  => "IconLightbulb",
            EntityKind.Mesh   => "IconCube",
            _                 => "IconEntity"
        };
    }
}
