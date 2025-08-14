using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
using Microsoft.Extensions.Logging;
using MsBox.Avalonia;
using MsBox.Avalonia.Base;
using MsBox.Avalonia.Enums;
using Somatic.Model;
using Somatic.ViewModels;
using System;
using System.IO;
using System.Linq;

namespace Somatic.Services {
    /// <summary>Servicio que realiza las configuraciones de aplicación.</summary>
    public class ConfigurationService : IConfigurationService {
#region Propiedades
        /// <summary>Proyectos disponibles en el sistema.</summary>
        public SomaticProjects Projects { get; private set; }
#endregion

#region Campos
        // Servicio de log.
        private readonly ILogger<ConfigurationService> _logger;
        // Servicio de serialización JSON.
        private readonly ISerializeService _serialize;
        // Servicio de traducción.
        private readonly ILocalizationService _localization;

        // Ubicación del archivo de estado de la ventana.
        private readonly string WindowStateFile = $"config{Path.DirectorySeparatorChar}WindowState.json";
        // Ubicación de la configuración de la aplicación.
        private readonly string ConfigurationFile = $"config{Path.DirectorySeparatorChar}config.soma";
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase.</summary>
        public ConfigurationService(
                ILogger<ConfigurationService> logger, 
                ISerializeService serialize, 
                ILocalizationService localization) {
            _logger = logger;
            _serialize = serialize;
            _localization = localization;

            if (!Directory.Exists("config")) Directory.CreateDirectory("config");

            if (File.Exists(ConfigurationFile)) {
                Projects = _serialize.FromFile<SomaticProjects>($"{AppDomain.CurrentDomain.BaseDirectory}{ConfigurationFile}");
                foreach (ProjectData project in Projects.Projects) {
                    try {
                        if (Directory.Exists(project.Path)) {
                            project.Icon = new Avalonia.Media.Imaging.Bitmap($"{project.Path}.soma{Path.DirectorySeparatorChar}icon.png");
                            project.Screenshot = new Avalonia.Media.Imaging.Bitmap($"{project.Path}.soma{Path.DirectorySeparatorChar}screenshot.png");
                        } else {
                            _logger.LogInformation($"No existe la ruta {project.Path}");
                        }
                    } catch {
                        _logger.LogError($"No se ha podido leer el proyecto {project.Name}");
                    }
                }
            } else {
                Projects = new SomaticProjects();
            }

            _localization = localization;
        }
#endregion

#region Métodos
        /// <summary>Guardamos el estado de la ventana.</summary>
        public bool SaveWindowState(Window window) {
            try {
                Screen currentScreen = GetCurrentScreen(window);
                int screenIndex = Array.IndexOf([.. window.Screens.All], currentScreen);

                SomaticWindowState state = new() {
                    X = window.Position.X,
                    Y = window.Position.Y,
                    Width = window.Width,
                    Height = window.Height,
                    IsMaximized = window.WindowState == WindowState.Maximized,
                    DisplayName = currentScreen.DisplayName!,
                    Bounds = currentScreen.Bounds,
                    Index = screenIndex
                };
                
                if (!Directory.Exists("config")) Directory.CreateDirectory("config");
                _serialize.ToFile<SomaticWindowState>(state, WindowStateFile);

                return true;

            } catch (Exception ex) {
                _logger.LogError($"No ha sido posible guardar la configuración: {ex.Message}");

                IMsBox<ButtonResult> dlg = MessageBoxManager.GetMessageBoxStandard("Error", $"{_localization.GetString("Msg_notwrite")}: {ex.Message}", ButtonEnum.Ok, Icon.Error);
                dlg.ShowAsync();

                return false;
            }
        }
        /// <summary>Carga del estado de la ventana.</summary>
        public bool LoadWindowState(Window window) {
            try {
                // Comprobamos que exista el archivo de estado previo de la ventana
                if (File.Exists(WindowStateFile)) {
                    SomaticWindowState state = _serialize.FromFile<SomaticWindowState>(WindowStateFile);
                    
                    Screen targetScreen = FindSavedScreen(window, state);
                    if (targetScreen != null) {
                        window.Position = new PixelPoint((int)(targetScreen.Bounds.X + (state.X - state.Bounds.X)), (int)(targetScreen.Bounds.Y + (state.Y - state.Bounds.Y)));
                    } else {
                        window.Position = new PixelPoint((int)state.X, (int)state.Y);
                    }
                    
                    window.Width = state.Width;
                    window.Height = state.Height;
                    
                    if (state.IsMaximized) window.WindowState = WindowState.Maximized;
                    
                    EnsureWindowIsOnScreen(window, targetScreen);
                }

                return true;
            } catch (Exception ex) {
                _logger.LogError($"No ha sido posible cargar la configuración: {ex.Message}");

                return false;
            }
        }
        /// <summary>Añade un proyecto.</summary>
        public void AddProject(CreateProjectViewModel vm) {
            try {
                Projects.Projects.Add(new ProjectData {
                    Name = vm.Name,
                    Path = $"{vm.Path}{vm.Name}{Path.DirectorySeparatorChar}",
                    Description = vm.Description,
                    LastTime = DateTime.Now
                });
                if (!Projects.Paths.Contains(vm.Path)) Projects.Paths.Add(vm.Path);
        
                SaveProjectStatus();
            } catch (Exception ex) {
                _logger.LogError($"No se ha podido guardar la configuración: {ex.ToString()}");
            }
        }
        /// <summary>Guarda el estado de un proyecto.</summary>
        public void SaveProjectStatus() => _serialize.ToFile(Projects, $"{AppDomain.CurrentDomain.BaseDirectory}{ConfigurationFile}");

        /// <summary>Obtiene la información de la pantalla.</summary>
        private Screen GetCurrentScreen(Window window) {
            // Tomamos todas las pantallas disponibles en el sistema
            Screen[] screens = [.. window.Screens.All];
            // Localizamos la pantalla obteniendo el punto central de la ventana...
            PixelPoint windowCenter = new(window.Position.X + (int)(window.Width / 2), window.Position.Y + (int)(window.Height / 2));
            foreach (Screen screen in screens) {
                if (screen.Bounds.Contains(windowCenter)) return screen;
            }
            // ...o mediante la posición de la ventana...
            foreach (Screen screen in screens) {
                if (screen.Bounds.Contains(window.Position)) return screen;
            }
            // ...o como ultima opción la pantalla primaría
            return screens.FirstOrDefault(s => s.IsPrimary) ?? screens[0];
        }
        private Screen FindSavedScreen(Window window, SomaticWindowState state) {
            // Tomamos todas las pantallas disponibles en el sistema
            Screen[] screens = [.. window.Screens.All];

            // Buscamos la pantalla por el nombre (es lo más fiable)
            Screen? screenByName = screens.FirstOrDefault(x => x.DisplayName == state.DisplayName);
            if (screenByName != null && Array.IndexOf(screens, screenByName) == state.Index) return screenByName;

            // Procedemos por el índice (puede cambiar al quitar un monitor o cambiarlo)
            if (state.Index >= 0 && state.Index < screens.Length) return screens[state.Index];

            // Lo menos fiable, la intersección de área
            foreach (Screen screen in screens) {
                // Ha de coincidir al menos un 50% del área de la pantalla
                PixelRect intersection = screen.Bounds.Intersect(state.Bounds);
                int intersectionArea = intersection.Width * intersection.Height;
                int originalArea = state.Bounds.Width * state.Bounds.Height;

                if (intersectionArea > (originalArea * 0.5)) return screen;
            }

            // Si no se encuentra, usar la pantalla primaria
            return screens.FirstOrDefault(x => x.IsPrimary) ?? screens[0];
        }
        private void EnsureWindowIsOnScreen(Window window, Screen? targetScreen = null) {
            // Tomamos todas las pantallas disponibles en el sistema
            Screen[] screens = [.. window.Screens.All];

            if (targetScreen == null) {
                bool isOnScreen = false;

                foreach (Screen screen in screens) {
                    if (screen.Bounds.Contains(window.Position)) {
                        isOnScreen = true;
                        break;
                    }
                }

                if (!isOnScreen && screens.Length > 0) {
                    // Usar pantalla primaria si está disponible
                    Screen primaryScreen = screens.FirstOrDefault(s => s.IsPrimary) ?? screens[0];
                    window.Position = new PixelPoint(primaryScreen.Bounds.X + 100, primaryScreen.Bounds.Y + 100);
                }
            }
        }
#endregion
    }
}
