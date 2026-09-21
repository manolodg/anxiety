using Dock.Model.Controls;
using Dock.Model.Core;
using Microsoft.Extensions.Logging;
using Somatic.Core.Workspace;

namespace Somatic.Infrastructure.Workspace {
    public sealed class FileWorkspaceLayoutService : IWorkspaceLayoutService {
        internal const int CurrentLayoutVersion = 1;

        private readonly IDockSerializer _serializer;
        private readonly ILogger<FileWorkspaceLayoutService> _logger;
        private readonly string _layoutFilePath;

        public FileWorkspaceLayoutService(IDockSerializer serializer, ILogger<FileWorkspaceLayoutService> logger) : this(serializer, logger, ResolveDefaultLayoutFilePath()) { }

        internal FileWorkspaceLayoutService(IDockSerializer serializer, ILogger<FileWorkspaceLayoutService> logger, string layoutFilePath) {
            _serializer = serializer;
            _logger = logger;
            _layoutFilePath = layoutFilePath;
        }

        private static string ResolveDefaultLayoutFilePath() {
            string applicationDataRoot = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData, Environment.SpecialFolderOption.Create);
            string directory = Path.Combine(applicationDataRoot, "Somatic");
            return Path.Combine(directory, "workspace.layout.json");
        }

        public IRootDock? TryLoadLayout() {
            if (!File.Exists(_layoutFilePath)) {
                _logger.LogInformation("No se ha encontrado un workspace guardado en {Path}; usando el layout definido.", _layoutFilePath);
                return null;
            }

            try {
                using FileStream stream = File.OpenRead(_layoutFilePath);
                WorkspaceLayoutEnvelope? envelope = _serializer.Load<WorkspaceLayoutEnvelope>(stream);

                if (envelope?.Layout is null) {
                    _logger.LogWarning("Layout del workspace en {Path} deserializado en un resultado vacío; usando el layout definido.", _layoutFilePath);
                    return null;
                }
                if (envelope.Version != CurrentLayoutVersion) {
                    _logger.LogWarning("Layout del workspace en {Path} tiene la versión {FoundVersion}, se esperaba {ExpectedVersion}; usando el layout definido.", _layoutFilePath, envelope.Version, CurrentLayoutVersion);
                    return null;
                }

                _logger.LogInformation("Layout del workspace restaurado desde {Path}.", _layoutFilePath);
                return envelope.Layout;
            } catch (Exception ex) {
                _logger.LogWarning(ex, "Fallo al cargar el layout del workspace desde {Path}; volviendo al layout definido.", _layoutFilePath);
                return null;
            }
        }

        public void SaveLayout(IRootDock layout) {
            try {
                string? directory = Path.GetDirectoryName(_layoutFilePath);
                if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);

                WorkspaceLayoutEnvelope envelope = new WorkspaceLayoutEnvelope { Version = CurrentLayoutVersion, Layout = layout };

                using FileStream stream = File.Create(_layoutFilePath);
                _serializer.Save(stream, envelope);
                _logger.LogInformation("Layout del workspace guardado en {Path}.", _layoutFilePath);
            } catch (Exception ex) {
                _logger.LogError(ex, "Fallo al guardar el layout del workspace en {Path}.", _layoutFilePath);
            }
        }

        public void ResetLayout() {
            try {
                if (File.Exists(_layoutFilePath)) File.Delete(_layoutFilePath);

                _logger.LogInformation("Layout workspace reiniciado; archivo persistido borrado de {Path}.", _layoutFilePath);
            } catch (Exception ex) {
                _logger.LogError(ex, "Fallo al reiniciar el layout del workspace en {Path}.", _layoutFilePath);
            }
        }
    }
}
