using Avalonia.Controls;
using Avalonia.Interactivity;
using Microsoft.Extensions.DependencyInjection;
using Somatic.Model;
using Somatic.Services;

namespace Somatic.Views {
    public partial class MainWindow : Window {
        public MainWindow() {
            InitializeComponent();
        }

        protected override void OnLoaded(RoutedEventArgs e) {
            EntityTree.Project = new Project {
                Name = "Proyecto de prueba",
                Description = "Proyecto de prueba",
                Path = "D:\\test.soma",
                ScenePaths = { "D:\\test.scene" },
                ActiveScenePath = "D:\\test.scene",
                ActiveScene = new Scene {
                    Name = "Escena 1",
                    IsActive = true,
                    Path = "D:\\test.scene",
                    Entities = {
                        new Entity {
                            Name = "Entidad 1",
                            IsActive = true,
                            Components = {
                                new TransformComponent {
                                    Name = "Transform Component",
                                    IsActive = true,
                                    Order = 1
                                }
                            }
                        },
                        new Entity {
                            Name = "Entidad 2",
                            IsActive = true,
                            Components = {
                                new TransformComponent {
                                    Name = "Transform Component",
                                    IsActive = true,
                                    Order = 1
                                }
                            }
                        }
                    }
                }
            };

            base.OnLoaded(e);
        }

        private void Button_Click(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
            ISerializeService serialize = Framework.ServiceProvider.GetRequiredService<ISerializeService>();

            Project project = new Project {
                Name = "Proyecto de prueba",
                Description = "Proyecto de prueba",
                Path = "D:\\test.soma",
                ScenePaths = { "D:\\test.scene" },
                ActiveScenePath = "D:\\test.scene",
                ActiveScene = new Scene {
                    Name = "Escena 1",
                    IsActive = true,
                    Path = "D:\\test.scene",
                    Entities = {
                        new Entity {
                            Name = "Entidad 1",
                            IsActive = true,
                            Components = {
                                new TransformComponent {
                                    Name = "Transform Component",
                                    IsActive = true,
                                    Order = 1,

                                }
                            }
                        }
                    }
                }
            };
            serialize.ToFile<Project>(project, "D:\\test.soma");

            Project projectRead = serialize.FromFile<Project>("D:\\test.soma");
        }
    }
}