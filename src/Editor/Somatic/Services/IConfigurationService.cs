using Avalonia.Controls;
using Somatic.Model;
using Somatic.ViewModels;

namespace Somatic.Services {
    /// <summary>Servicio de configuraciones.</summary>
    public interface IConfigurationService {
#region Propiedades
        /// <summary>Proyectos disponibles en el sistema.</summary>
        SomaticProjects Projects { get; }
#endregion

#region Métodos
        /// <summary>Carga del estado de la ventana.</summary>
        bool LoadWindowState(Window window);
        /// <summary>Guardamos el estado de la ventana.</summary>
        bool SaveWindowState(Window window);
        /// <summary>Añade un proyecto.</summary>
        void AddProject(CreateProjectViewModel vm);
        /// <summary>Guarda el estado de un proyecto.</summary>
        void SaveProjectStatus();
#endregion
    }
}