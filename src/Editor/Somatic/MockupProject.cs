using Somatic.Model;

namespace Somatic {
    public class MockupProject : Project {
        public MockupProject() {
            ActiveScene = new Scene {
                Name = "Escena"
            };
            var entities = ActiveScene.Entities;
            var entity1 = new Entity { Name = "Entidad raíz 1" };
            var entity11 = new Entity { Name = "Entidad hija 1 de 1" };
            var entity12 = new Entity { Name = "Entidad hija 2 de 1" };
            entity1.Children.Add(entity11);
            entity1.Children.Add(entity12);

            var entity2 = new Entity { Name = "Entidad raíz 2" };
            var entity21 = new Entity { Name = "Entidad hija 1 de 2" };
            entity2.Children.Add(entity21);

            var entity211 = new Entity { Name = "Entidad nieta 1 de 2" };
            entity21.Children.Add(entity211);

            entities.Add(entity1);
            entities.Add(entity2);
        }
    }
}
