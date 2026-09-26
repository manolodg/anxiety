using Somatic.Core.Scene.Components;

namespace Somatic.Core.Scene {
    /// <summary>Crea entidades con los componentes por defecto de su tipo (además de la transformación, que tienen todas).</summary>
    public static class EntityFactory {
        public static Entity Create(EntityKind kind, string name) {
            Entity entity = new Entity(kind, name);

            switch (kind) {
                case EntityKind.Camera:
                    entity.AddComponent(new CameraComponent());
                    break;
                case EntityKind.Light:
                    entity.AddComponent(new LightComponent());
                    break;
                case EntityKind.Mesh:
                    entity.AddComponent(new MeshRendererComponent());
                    break;
            }

            return entity;
        }
    }
}
