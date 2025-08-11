using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Services;
using System;

namespace Somatic.Extensions {
    /// <summary>Extensión Markup para realizar la localización en axaml.</summary>
    public class TranslateMarkupExtension : MarkupExtension {
#region Propiedades
        /// <summary>Clave de la localización.</summary>
        public string Key { get; set; }
#endregion

#region Constructores
        /// <summary>Crea una nueva instancia de la clase <see cref="TranslateMarkupExtension"/>.</summary>
        /// <param name="key">Clave de la localización.</param>
        public TranslateMarkupExtension(string key) => Key = key;
#endregion

#region Métodos
        /// <summary>Devuelve el valor de la localización.</summary>
        public override object ProvideValue(IServiceProvider serviceProvider) {
            if (string.IsNullOrEmpty(Key)) return string.Empty;

            ILocalizationService? localizationService = Framework.ServiceProvider?.GetRequiredService<ILocalizationService>();
            if (localizationService == null) return Key;

            return localizationService.GetString(Key);
        }
#endregion
    }
}
