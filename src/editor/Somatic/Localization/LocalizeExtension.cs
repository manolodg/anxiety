using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Core.Localization;

namespace Somatic.Localization {
    /// <summary>
    /// <c>{loc:Localize Clave}</c>: texto localizado resuelto una vez al cargar la vista. En el diseñador (sin
    /// servicios) devuelve la propia clave.
    /// </summary>
    public sealed class LocalizeExtension : MarkupExtension {
        public LocalizeExtension() { }

        public LocalizeExtension(string key) {
            Key = key;
        }

        public string Key { get; set; } = string.Empty;

        public override object ProvideValue(IServiceProvider serviceProvider) => Get(Key);

        public static string Get(string key) => App.Services?.GetService<ILocalizationService>()?.GetString(key) ?? key;
    }
}
