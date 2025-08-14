using FluentValidation;
using Somatic.Services;
using Somatic.ViewModels;
using System.IO;
using System.Linq;

namespace Somatic.Models {
    /// <summary>Configuración de la validación.</summary>
    public class CreateProjectValidator : AbstractValidator<CreateProjectViewModel> {
        /// <summary>Reglas de validación del sistema.</summary>
        public CreateProjectValidator(ILocalizationService localization, IConfigurationService config) {
            RuleFor(x => x.Name)
                .NotEmpty().WithMessage(localization.GetString("Rule_mandatory"))
                .Custom((x, ctx) => {
                    if (x != null && x.IndexOfAny(Path.GetInvalidFileNameChars()) != -1) {
                        ctx.AddFailure(localization.GetString("Rule_nomatch"));
                    } else if (x != null && config.Projects.Projects.Any(y => y.Name == x)) {
                        ctx.AddFailure(localization.GetString("Rule_exists"));
                    }
                });
            RuleFor(x => x.Path)
                .NotEmpty().WithMessage(localization.GetString("Rule_mandatory"))
                .Custom((x, ctx) => {
                    if (!Path.EndsInDirectorySeparator(x)) x += Path.DirectorySeparatorChar;
                    x += $"{ctx.InstanceToValidate.Name}{Path.DirectorySeparatorChar}";

                    if (x.IndexOfAny(Path.GetInvalidPathChars()) != -1) ctx.AddFailure(localization.GetString("Rule_nomatch"));
                    if (Directory.Exists(x) && Directory.EnumerateFileSystemEntries(x).Any()) ctx.AddFailure(localization.GetString("Rule_folderexists"));
                });
        }
    }
}
