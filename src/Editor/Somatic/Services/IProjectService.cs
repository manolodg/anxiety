using Somatic.Model;
using Somatic.ViewModels;

namespace Somatic.Services {
    /// <summary>Servicio para la gestión de los proyectos.</summary>
    public interface IProjectService {
#region Métodos
        /// <summary>Obtener las plantillas presentes en la aplicación.</summary>
        TemplateProject[] GetTemplates();
        /// <summary>Creación de un proyecto.</summary>
        void CreateProject(CreateProjectViewModel vm);
        /// <summary>Carga de un proyecto.</summary>
        bool LoadProject(ProjectData project);
        /// <summary>Guardar un proyecto.</summary>
        bool SaveProject();
#endregion
    }
}
