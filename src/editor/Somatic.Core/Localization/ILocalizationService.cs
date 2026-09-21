using System.Globalization;

namespace Somatic.Core.Localization {
    public interface ILocalizationService {
        CultureInfo CurrentCulture { get; }

        string GetString(string key);
        void SetCulture(CultureInfo culture);
    }
}
