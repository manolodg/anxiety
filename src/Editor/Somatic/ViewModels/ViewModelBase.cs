using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Services;

namespace Somatic.ViewModels {
    /// <summary>Base de los ViewModel para que se de soporte a CommunityMVVM.</summary>
    public class ViewModelBase : ObservableObject {
#region Globales
        protected readonly ILocalizationService TranslateService;
#endregion

#region Propiedades
        protected ViewModelBase() => TranslateService = Framework.ServiceProvider.GetRequiredService<ILocalizationService>();
#endregion

#region Métodos
        /// <summary>Obtiene la clave traducida según el idioma actual.</summary>
        /// <param name="key">Clave a traducir.</param>
        /// <returns>Si la clave existe devuelve la traducción, sino devuelve la clave.</returns>
        public string GetLocalizedString(string key) => TranslateService.GetString(key);
#endregion
    }
}
