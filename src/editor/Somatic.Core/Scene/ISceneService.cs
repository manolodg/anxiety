using System.ComponentModel;
using Component = Somatic.Core.Scene.Components.Component;

namespace Somatic.Core.Scene {
    /// <summary>
    /// Escena abierta en el editor y entidad seleccionada. Lo comparten la jerarquía, el inspector y el viewport.
    /// Todas las operaciones de estructura pasan por aquí para quedar en el historial de deshacer; las ediciones
    /// de propiedades del modelo se registran solas.
    /// </summary>
    public interface ISceneService : INotifyPropertyChanged {
        SceneGraph ActiveScene { get; }

        Entity? SelectedEntity { get; set; }

        /// <summary>Crea una entidad del tipo indicado bajo <paramref name="parent"/> (o en la raíz), con nombre único entre sus hermanas, y la selecciona.</summary>
        Entity CreateEntity(EntityKind kind, Entity? parent = null);

        /// <summary>Elimina la entidad y sus hijos. Si la selección estaba dentro, pasa al padre.</summary>
        void DeleteEntity(Entity entity);

        /// <summary>Reparenta/reordena (ver <see cref="SceneGraph.TryResolveMove"/>). Devuelve <c>false</c> si el destino no es válido o no hay cambio.</summary>
        bool MoveEntity(Entity entity, Entity? newParent, int index = -1);

        /// <summary>Cambia el nombre; ignora nombres vacíos. Devuelve <c>true</c> si se aplicó.</summary>
        bool RenameEntity(Entity entity, string name);

        /// <summary>Añade el componente al final de la entidad. Devuelve <c>false</c> si ya tenía uno de ese tipo.</summary>
        bool AddComponent(Entity entity, Component component);

        /// <summary>Quita el componente de su entidad. Devuelve <c>false</c> si no se puede quitar.</summary>
        bool RemoveComponent(Component component);
    }
}
