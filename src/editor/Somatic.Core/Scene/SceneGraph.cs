using System.Collections.ObjectModel;

namespace Somatic.Core.Scene {
    /// <summary>Árbol de entidades de una escena. Único punto por el que se añaden, quitan y reparentan entidades.</summary>
    public sealed class SceneGraph {
        private readonly ObservableCollection<Entity> _rootEntities = [];

        public ReadOnlyObservableCollection<Entity> RootEntities { get; }

        public SceneGraph() {
            RootEntities = new ReadOnlyObservableCollection<Entity>(_rootEntities);
        }

        /// <summary>Añade una entidad suelta como raíz o como hija de <paramref name="parent"/>. Con <paramref name="index"/> negativo se añade al final.</summary>
        public void Add(Entity entity, Entity? parent = null, int index = -1) {
            if (entity.Parent is not null || _rootEntities.Contains(entity)) throw new InvalidOperationException($"La entidad '{entity.Name}' ya está en la escena.");
            Insert(entity, parent, index);
        }

        public void Remove(Entity entity) {
            if (entity.Parent is not null) entity.Parent.RemoveChild(entity);
            else _rootEntities.Remove(entity);
        }

        /// <summary><c>false</c> si <paramref name="newParent"/> es la propia entidad o uno de sus descendientes.</summary>
        public static bool CanMove(Entity entity, Entity? newParent) => newParent is null || (newParent != entity && !entity.IsAncestorOf(newParent));

        /// <summary>
        /// Traduce una posición de destino "tal y como se ve antes de mover" (lo que indica el usuario al soltar
        /// entre dos filas; negativo = al final) a la posición que tendrá la entidad una vez movida.
        /// </summary>
        /// <returns><c>false</c> si el destino no es válido o la entidad ya está ahí.</returns>
        public bool TryResolveMove(Entity entity, Entity? newParent, int index, out int finalIndex) {
            finalIndex = -1;
            if (!CanMove(entity, newParent)) return false;

            IReadOnlyList<Entity> target = newParent?.Children ?? (IReadOnlyList<Entity>)RootEntities;
            if (index < 0 || index > target.Count) index = target.Count;

            if (entity.Parent != newParent) {
                finalIndex = index;
                return true;
            }

            // Misma lista: al quitarla primero, las posiciones por detrás de la suya retroceden una.
            int oldIndex = IndexOf(entity);
            if (index > oldIndex) --index;
            finalIndex = index;
            return index != oldIndex;
        }

        /// <summary>Mueve la entidad (con sus hijos) para que quede en la posición <paramref name="finalIndex"/> de <paramref name="newParent"/> (o de la raíz).</summary>
        public void MoveTo(Entity entity, Entity? newParent, int finalIndex) {
            if (!CanMove(entity, newParent)) throw new InvalidOperationException("Una entidad no puede colgar de sí misma ni de sus descendientes.");

            Remove(entity);
            Insert(entity, newParent, finalIndex);
        }

        /// <summary>Atajo de <see cref="TryResolveMove"/> + <see cref="MoveTo"/>. Devuelve <c>false</c> si no hubo cambio.</summary>
        public bool Move(Entity entity, Entity? newParent, int index = -1) {
            if (!CanMove(entity, newParent)) throw new InvalidOperationException("Una entidad no puede colgar de sí misma ni de sus descendientes.");
            if (!TryResolveMove(entity, newParent, index, out int finalIndex)) return false;

            MoveTo(entity, newParent, finalIndex);
            return true;
        }

        /// <summary>Lista a la que pertenece la entidad: los hijos de su padre o las raíces.</summary>
        public IReadOnlyList<Entity> SiblingsOf(Entity entity) => entity.Parent?.Children ?? (IReadOnlyList<Entity>)RootEntities;

        /// <summary>Posición de la entidad entre sus hermanas, o -1 si no está en la escena.</summary>
        public int IndexOf(Entity entity) {
            IReadOnlyList<Entity> siblings = SiblingsOf(entity);
            for (int i = 0; i < siblings.Count; ++i) {
                if (siblings[i] == entity) return i;
            }
            return -1;
        }

        /// <summary><c>true</c> si la entidad cuelga de alguna raíz de esta escena.</summary>
        public bool Contains(Entity entity) {
            Entity root = entity;
            while (root.Parent is not null) root = root.Parent;
            return _rootEntities.Contains(root);
        }

        /// <summary>Recorre en profundidad todas las entidades de la escena.</summary>
        public IEnumerable<Entity> Traverse() {
            Stack<Entity> pending = new Stack<Entity>(_rootEntities.Reverse());
            while (pending.Count > 0) {
                Entity entity = pending.Pop();
                yield return entity;
                for (int i = entity.Children.Count - 1; i >= 0; --i) pending.Push(entity.Children[i]);
            }
        }

        private void Insert(Entity entity, Entity? parent, int index) {
            if (parent is not null) {
                parent.InsertChild(entity, index);
                return;
            }
            _rootEntities.Insert(index < 0 || index > _rootEntities.Count ? _rootEntities.Count : index, entity);
        }
    }
}
