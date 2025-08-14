using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using FluentValidation;
using FluentValidation.Results;
using Somatic.Model;
using Somatic.Services;
using Somatic.Views;
using System;
using System.Collections;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;

namespace Somatic.ViewModels {
    public partial class CreateProjectViewModel : PageViewModel, INotifyDataErrorInfo {
        public EngineModes EngineMode { get; set; }
        public EngineTypes EngineType { get; set; }

        [ObservableProperty] private ObservableCollection<TemplateProject> _templates;
        [ObservableProperty] private TemplateProject _selectedProject = null!;
        [ObservableProperty] private ObservableCollection<string> _paths;

        [ObservableProperty] private string _name;
        [ObservableProperty] private string _path;
        [ObservableProperty] private string _description;

        [ObservableProperty] private string[] _engineTypes;
        [ObservableProperty] private string[] _engineModes;

        private readonly IValidator<CreateProjectViewModel> _validator;
        private readonly IProjectService _service;
        private readonly TemplateProject[] _baseTemplates;
        private readonly IConfigurationService _config;

        private ValidationResult? _result;

        public bool HasErrors => !_result!.IsValid;

        public event EventHandler<DataErrorsChangedEventArgs>? ErrorsChanged;

#pragma warning disable CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
        public CreateProjectViewModel() {
            Title = GetLocalizedString("CrPr_title");

            EngineTypes = Enum.GetNames(typeof(EngineTypes)).Select(x => GetLocalizedString(x)).ToArray();
            EngineModes = Enum.GetNames(typeof(EngineModes)).Select(x => GetLocalizedString(x)).ToArray();

            if (Design.IsDesignMode) {
                Templates = new ObservableCollection<TemplateProject> {
                    new TemplateProject { Name = "Proyecto 1", Description = "Primer proyecto" },
                    new TemplateProject { Name = "Proyecto 2", Description = "Segundo proyecto" },
                    new TemplateProject { Name = "Proyecto 3", Description = "Tercer proyecto" }
                };

                SelectedProject = Templates[0];
            }
        }
#pragma warning restore CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
        public CreateProjectViewModel(IValidator<CreateProjectViewModel> validator, IProjectService service, IConfigurationService config) {
            _validator = validator;
            _service = service;
            _config = config;

            _result = _validator.Validate(this);

            _baseTemplates = _service.GetTemplates();

            Title = GetLocalizedString("CrPr_title");
            Templates = [.. _baseTemplates];

            Name = GetLocalizedString("NewProject");
            Path = $"{Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)}{System.IO.Path.DirectorySeparatorChar}SomaticProjects{System.IO.Path.DirectorySeparatorChar}";
            Description = string.Empty;

            Paths = new ObservableCollection<string>(_config.Projects.Paths);

            EngineTypes = Enum.GetNames(typeof(EngineTypes)).Select(x => GetLocalizedString(x)).ToArray();
            EngineModes = Enum.GetNames(typeof(EngineModes)).Select(x => GetLocalizedString(x)).ToArray();

            SelectedProject = Templates[0];
        }

        public IEnumerable GetErrors(string? propertyName) {
            if (_result != null && !_result.IsValid) {
                string[] messages = [.. _result.Errors.Where(x => x.PropertyName == propertyName).Select(x => x.ErrorMessage)];
                if (messages.Length != 0) return messages;
            }

            return null!;
        }

        protected override void OnPropertyChanged(PropertyChangedEventArgs e) {
            base.OnPropertyChanged(e);

            if (e.PropertyName == nameof(Name) || e.PropertyName == nameof(Path)) {
                bool hasErrors = HasErrors;
                _result = _validator.Validate(this);
                if (hasErrors != HasErrors) {
                    ErrorsChanged?.Invoke(this, new DataErrorsChangedEventArgs(e.PropertyName));
                    if (e.PropertyName == nameof(Name)) ErrorsChanged?.Invoke(this, new DataErrorsChangedEventArgs(nameof(Path)));

                    OnPropertyChanged(nameof(HasErrors));
                }
            }
        }

        [RelayCommand]
        private void Accept(DialogView view) {
            _result = _validator.Validate(this);
            if (_result.IsValid) {
                _service.CreateProject(this);

                view._mainAction?.Invoke();

                view.Close();
            } else {
                ErrorsChanged?.Invoke(this, new DataErrorsChangedEventArgs(nameof(HasErrors)));
            }
        }
        [RelayCommand]
        private void Cancel(DialogView view) {
            view.Close();
        }
    }
}
