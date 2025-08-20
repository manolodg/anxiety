using Somatic.Model;
using System.Collections.ObjectModel;

namespace Somatic.Services {
    /// <summary>Servicio que realiza todas las funciones relativas a las entidades.</summary>
    public class EntityService : IEntityService {
#region Métodos
        /// <summary>Crea una nueva entidad.</summary>
        /// <param name="entities">Entidades sobre las que crear la nueva entidad.</param>
        /// <param name="name">Nombre que recibe la nueva entidad.</param>
        /// <returns>Entidad creada.</returns>
        public Entity CreateEntity(ObservableCollection<BaseEntity> entities, string name) {
            Entity result = new Entity(name, true);
            entities.Add(result);

            return result;
        }
#endregion
    }
}
