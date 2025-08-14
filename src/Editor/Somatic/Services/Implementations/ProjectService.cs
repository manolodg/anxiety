using AutoMapper;
using Microsoft.Extensions.Logging;
using MsBox.Avalonia;
using MsBox.Avalonia.Base;
using MsBox.Avalonia.Enums;
using Somatic.Model;
using Somatic.ViewModels;
using System;
using System.Collections.Generic;
using System.IO;

namespace Somatic.Services {
    public class ProjectService(ISerializeService serialize, ILogger<ProjectService> logger, IConfigurationService config, Project project, IMapper mapper, ILocalizationService localization) : IProjectService {
        private const string TemplateFile = "template.soma";
        private const string Extension = ".soma";

        public TemplateProject[] GetTemplates() {
            List<TemplateProject> resultado = [];

            string path = Path.Combine(Directory.GetCurrentDirectory(), "Templates");
            foreach (var file in Directory.GetFiles(path, TemplateFile, SearchOption.AllDirectories)) {
                TemplateProject template = serialize.FromFile<TemplateProject>(file);
                template.IconPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, "Icon.png"));
                template.Icon = new Avalonia.Media.Imaging.Bitmap(template.IconPath);
                template.ScreenshotPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, "Screenshot.png"));
                template.Screenshot = new Avalonia.Media.Imaging.Bitmap(template.ScreenshotPath);
                template.TemplateProjectPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, "project.soma"));

                resultado.Add(template);
            }

            return resultado.ToArray();
        }

        public void CreateProject(CreateProjectViewModel vm) {
            try {
                if (!Path.EndsInDirectorySeparator(vm.Path)) vm.Path += Path.DirectorySeparatorChar;
                string path = $"{vm.Path}{vm.Name}{Path.DirectorySeparatorChar}";

                // Creamos el directorio del proyecto en la ubicación indicada
                if (!Directory.Exists(path)) Directory.CreateDirectory(path);
                // Creamos todas las carpetas que contiene la plantilla
                foreach (string folder in vm.SelectedProject.Folders) {
                    string transalted = folder.Replace('\\', System.IO.Path.DirectorySeparatorChar);
                    Directory.CreateDirectory(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(path)!, transalted)));
                }

                // Creamos la carpeta con el contenido
                DirectoryInfo dirInfo = new DirectoryInfo($"{path}.soma{Path.DirectorySeparatorChar}");
                dirInfo.Attributes |= FileAttributes.Hidden;
                // Copiamos el contenido en la carpeta oculta
                File.Copy(vm.SelectedProject.IconPath, Path.GetFullPath(Path.Combine(dirInfo.FullName, "icon.png")));
                File.Copy(vm.SelectedProject.ScreenshotPath, Path.GetFullPath(Path.Combine(dirInfo.FullName, "screenshot.png")));

                Project projectLocal = new Project {
                    Name = vm.Name,
                    Path = path,
                    Description = vm.Description,
                    EngineType = vm.EngineType,
                    EngineMode = vm.EngineMode
                };
                serialize.ToFile<Project>(projectLocal, Path.GetFullPath(Path.Combine(path, $"{vm.Name}{Extension}")));

                // Dejamos cargado el proyecto que va a funcionar en toda la aplicación
                AssignProject(projectLocal);

                // Añadimos el proyecto a la lista de proyectos
                config.AddProject(vm);
            } catch (Exception ex) {
                logger.LogError(ex, "No se ha podido crear el proyecto");
            }
        }

        public bool LoadProject(ProjectData projectData) {
            string path = $"{projectData.Path}{projectData.Name}{Extension}";
            if (!Directory.Exists(projectData.Path)) {
                logger.LogError($"No existe el directorio {projectData.Path}");

                IMsBox<ButtonResult> dlg = MessageBoxManager.GetMessageBoxStandard("Error", $"{localization.GetString("Msg_notfolder")} {projectData.Path}", ButtonEnum.Ok, Icon.Error);
                dlg.ShowAsync();

                return false;
            }

            try {
                Project projectLocal = serialize.FromFile<Project>(path);
                if (projectLocal == null) throw new Exception("Error al deserializar el archivo");

                AssignProject(projectLocal);
            } catch (Exception ex) {
                logger.LogError($"Error al procesar el archivo {path}: {ex.Message}");

                IMsBox<ButtonResult> dlg = MessageBoxManager.GetMessageBoxStandard("Error", $"{localization.GetString("Msg_badfile")} {path}", ButtonEnum.Ok, Icon.Error);
                dlg.ShowAsync();

                return false;
            }

            return true;
        }

        public bool SaveProject() {
            try {
                serialize.ToFile<Project>(project, Path.GetFullPath(Path.Combine(project.Path, $"{project.Name}{Extension}")));
                return true;
            } catch (Exception ex) {
                logger.LogError(ex, "No se ha podido guardar el proyecto");
                return false;
            }
        }

        private void AssignProject(Project origin) => mapper.Map(origin, project);
    }
}
