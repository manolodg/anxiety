using Avalonia.Controls;
using Avalonia.Data;
using Dock.Avalonia.Controls;
using Dock.Model.Controls;
using Dock.Model.Core;
using Dock.Model.Mvvm;
using Dock.Model.Mvvm.Controls;
using Microsoft.Extensions.DependencyInjection;
using Somatic.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Somatic.Model {
    /// <summary>Aquí se hace la composición de los Dock de la pantalla principal.</summary>
    public class MainDockFactory(IServiceProvider services) : Factory {
#region Campos
        // Soporte de datos... realmente no es útil para nosotros
        private readonly object _context = new();

        // Esto es para soportar los documentos
        private IDocumentDock? _documentDock;
#endregion

#region Métodos
    #region Métodos de Inicio
        public DocumentDock FindDocumentDockById(IDock dock, string id) {
            Queue<IDock> queue = new Queue<IDock>();
            queue.Enqueue(dock);

            while (queue.Count > 0) {
                IDock currentDock = queue.Dequeue();
                if (currentDock is DocumentDock documentDock && documentDock.Id == id) return documentDock;

                if (currentDock.VisibleDockables != null) {
                    foreach (IDock dockable in currentDock.VisibleDockables.OfType<IDock>()) {
                        queue.Enqueue(dockable);
                    }
                }
            }

            return null!;
        }

        public override IRootDock CreateLayout() {
            ProportionalDock mainLayout = new ProportionalDock {
                Id = "MainLayout",
                Orientation = Orientation.Horizontal,
                VisibleDockables = CreateMainLayout()
            };

            MainViewModel mainView = new MainViewModel {
                Id = "Main",
                ActiveDockable = mainLayout,
                VisibleDockables = CreateList<IDockable>(mainLayout)
            };

            IRootDock root = CreateRootDock();
            root.Id = "Root";
            root.ActiveDockable = mainView;
            root.DefaultDockable = mainView;
            root.VisibleDockables = CreateList<IDockable>(mainView);

            return root;
        }
        public override void InitLayout(IDockable layout) {
            this.ContextLocator = new Dictionary<string, Func<object?>> {
                [nameof(IRootDock)] = () => _context,
                [nameof(IProportionalDock)] = () => _context,
                [nameof(IDocumentDock)] = () => _context,
                [nameof(IToolDock)] = () => _context,
                [nameof(IProportionalDockSplitter)] = () => _context,
                [nameof(IDockWindow)] = () => _context,
                [nameof(IDocument)] = () => _context,
                [nameof(ITool)] = () => _context,
                ["DocumentsPane"] = () => _context,
                ["MainLayout"] = () => _context,
                ["TopPane"] = () => _context,
                ["BottomPane"] = () => _context,
                ["Main"] = () => _context,
                ["TopPaneSplitter"] = () => _context,
                ["Bottom"] = () => _context
            };
            this.HostWindowLocator = new Dictionary<string, Func<IHostWindow?>> {
                [nameof(IDockWindow)] = () => {
                    HostWindow hostWindow = new HostWindow { [!Window.TitleProperty] = new Binding("ActiveDockable.Title") };
                    return hostWindow;
                }
            };
            this.DockableLocator = new Dictionary<string, Func<IDockable?>> { };

            base.InitLayout(layout);

            if (_documentDock is { }) {
                this.SetActiveDockable(_documentDock);
                this.SetFocusedDockable(_documentDock, _documentDock.VisibleDockables?.FirstOrDefault());
            }
        }

        private IList<IDockable> CreateMainLayout() {
            IList<IDockable> editorPane = CreateEditor();
            IList<IDockable> outputPane = CreateOutput();

            ProportionalDock topPane = new ProportionalDock {
                Id = "TopPane",
                Orientation = Orientation.Vertical,
                VisibleDockables = CreateList<IDockable>(
                    new ProportionalDock {
                        Id = "LeftPane",
                        Orientation = Orientation.Horizontal,
                        VisibleDockables = CreateList<IDockable>(
                            new ToolDock {
                                Id = "SolutionExplorerPane",
                                Proportion = 0.2,
                                Alignment = Alignment.Left,
                                // Panel del Explorador de soluciones ---------------------------------------------
                                VisibleDockables = CreateSolutionExplorer()
                            },
                            new ProportionalDockSplitter { Id = "LeftSplitter" },
                            new DocumentDock {
                                Id = "DocumentPane",
                                CanCreateDocument = false,
                                // Editor -------------------------------------------------------------------------
                                ActiveDockable = editorPane.First(),
                                VisibleDockables = editorPane
                            })
                    },
                    new ProportionalDockSplitter { Id = "BottomSplitter" },
                    new ToolDock {
                        Id = "BottomPane",
                        Proportion = 0.2,
                        Alignment = Alignment.Bottom,
                        // Panel de salida ------------------------------------------------------------------------
                        ActiveDockable = outputPane.First(),
                        VisibleDockables = outputPane
                    })
            };

            ToolDock propertiesPane = new ToolDock { 
                Id = "PropertiesPane",
                Proportion = 0.25,
                Alignment = Alignment.Left,
                VisibleDockables = CreateProperties()
            };

            return CreateList<IDockable>(
                topPane,
                new ProportionalDockSplitter { Id = "RightSplitter" },
                propertiesPane);
        }

    #endregion

    #region Métodos para poblar los dock
        private IList<IDockable> CreateSolutionExplorer() {
            // Explorador de soluciones ---------------------------------------------------------------------------
            return CreateList<IDockable>(
                services.GetRequiredService<SolutionExplorerViewModel>());
            // --------------------------------------------------------------------------- Explorador de soluciones
        }
        private IList<IDockable> CreateEditor() {
            // Editor ---------------------------------------------------------------------------------------------
            return CreateList<IDockable>(
                new Document { Id = "Document" });
            // --------------------------------------------------------------------------------------------- Editor
        }
        private IList<IDockable> CreateOutput() {
            // Salida ---------------------------------------------------------------------------------------------
            return CreateList<IDockable>(
                services.GetRequiredService<FileExplorerViewModel>(),
                services.GetRequiredService<LoggerViewModel>(),
                services.GetRequiredService<ClipboardViewModel>()
                );
            // --------------------------------------------------------------------------------------------- Salida
        }
        private IList<IDockable> CreateProperties() {
            // Propiedades ----------------------------------------------------------------------------------------
            return CreateList<IDockable>(
                services.GetRequiredService<InformationViewModel>());
            // --------------------------------------------------------------------------------------- Propiedades1
        }
    #endregion
#endregion
    }
}
