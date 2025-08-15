using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Model;
using Somatic.Services;
using Somatic.Views;
using System;
using System.Collections.ObjectModel;
using System.Linq;

namespace Somatic.ViewModels {
    public partial class OpenProjectViewModel : PageViewModel {
        private readonly IConfigurationService _config;
        private readonly IProjectService _service;

        private ProjectData[] _baseProjects;

        [ObservableProperty] private ObservableCollection<ProjectData> _projects;
        [ObservableProperty] private string _searchText = null!;
        [ObservableProperty] private ProjectData _selectedProject = null!;

#pragma warning disable CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
        public OpenProjectViewModel() {
            Title = GetLocalizedString("OpPr_Title");

            if (Design.IsDesignMode) {
                Projects = [
                    new() { Path = "\\Templates\\Empty\\", Name = "Proyecto 1", LastTime = DateTime.Now },
                    new() { Path = "\\Templates\\Empty\\", Name = "Proyecto 2", LastTime = DateTime.Now },
                    new() { Path = "\\Templates\\Empty\\", Name = "Proyecto 3", LastTime = DateTime.Now }
                ];

                SelectedProject = Projects[0];
            }
        }
#pragma warning restore CS8618 // Un campo que no acepta valores NULL debe contener un valor distinto de NULL al salir del constructor. Considere la posibilidad de agregar el modificador "required" o declararlo como un valor que acepta valores NULL.
        public OpenProjectViewModel(IConfigurationService config, IProjectService service) {
            _config = config;
            _service = service;

            _baseProjects = [.. _config.Projects.Projects.OrderByDescending(x => x.LastTime)];

            Title = GetLocalizedString("OpPr_Title");
            Projects = [.. _baseProjects];

            SelectedProject = Projects[0];
        }

        [RelayCommand]
        private void Search() {
            if (!string.IsNullOrEmpty(SearchText)) Projects = [.. _baseProjects.Where(x => x.Name.Contains(SearchText)).OrderByDescending(x => x.LastTime)];
        }
        [RelayCommand]
        private void NewProject(DialogView view) {
            (view.DataContext as DialogViewModel)!.CurrentPage = Framework.ServiceProvider.GetRequiredService<CreateProjectViewModel>();
        }
        [RelayCommand]
        private void Accept(DialogView view) {
            if (!_service.LoadProject(SelectedProject)) {
                _config.Projects.Projects.Remove(SelectedProject);
                _config.SaveProjectStatus();

                if (!_config.Projects.Projects.Any()) NewProject(view);

                _baseProjects = [.. _config.Projects.Projects.OrderByDescending(x => x.LastTime)];
            } else {
                view._mainAction?.Invoke();

                view.Close();
            }
        }
    }
}
