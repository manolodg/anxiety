using Somatic.Core.Scene;

namespace Somatic.Tests.Scene {
    public sealed class SceneGraphTests {
        private static (SceneGraph Graph, Entity A, Entity B, Entity C) CreateRoots() {
            SceneGraph graph = new SceneGraph();
            Entity a = new Entity(EntityKind.Empty, "A");
            Entity b = new Entity(EntityKind.Empty, "B");
            Entity c = new Entity(EntityKind.Empty, "C");
            graph.Add(a);
            graph.Add(b);
            graph.Add(c);
            return (graph, a, b, c);
        }

        private static string Names(IEnumerable<Entity> entities) => string.Join(",", entities.Select(e => e.Name));

        [Fact]
        public void Move_DownWithinSameList_AccountsForRemoval() {
            (SceneGraph graph, Entity a, _, _) = CreateRoots();

            // Soltar A "entre B y C" = índice 2 visto antes de mover.
            Assert.True(graph.Move(a, null, 2));

            Assert.Equal("B,A,C", Names(graph.RootEntities));
        }

        [Fact]
        public void Move_UpWithinSameList() {
            (SceneGraph graph, _, _, Entity c) = CreateRoots();

            Assert.True(graph.Move(c, null, 0));

            Assert.Equal("C,A,B", Names(graph.RootEntities));
        }

        [Theory]
        [InlineData(0)] // antes de sí misma
        [InlineData(1)] // justo después de sí misma
        public void Move_ToOwnPosition_IsNoOp(int index) {
            (SceneGraph graph, Entity a, _, _) = CreateRoots();

            Assert.False(graph.Move(a, null, index));
            Assert.Equal("A,B,C", Names(graph.RootEntities));
        }

        [Fact]
        public void Move_IntoAnotherEntity_Reparents() {
            (SceneGraph graph, Entity a, Entity b, _) = CreateRoots();

            Assert.True(graph.Move(b, a));

            Assert.Same(a, b.Parent);
            Assert.Equal("A,C", Names(graph.RootEntities));
            Assert.Equal("B", Names(a.Children));
            Assert.True(graph.Contains(b));
        }

        [Fact]
        public void CanMove_RejectsSelfAndDescendants() {
            (SceneGraph graph, Entity a, Entity b, _) = CreateRoots();
            graph.Move(b, a);

            Assert.False(SceneGraph.CanMove(a, a));
            Assert.False(SceneGraph.CanMove(a, b));
            Assert.Throws<InvalidOperationException>(() => graph.Move(a, b));
            Assert.True(SceneGraph.CanMove(b, null));
        }

        [Fact]
        public void Traverse_IsDepthFirstInOrder() {
            (SceneGraph graph, Entity a, Entity b, Entity c) = CreateRoots();
            graph.Move(c, a, 0);
            Entity d = new Entity(EntityKind.Empty, "D");
            graph.Add(d, c);

            Assert.Equal("A,C,D,B", Names(graph.Traverse()));
            Assert.Equal(1, graph.IndexOf(b));
            Assert.Equal(0, graph.IndexOf(c));
        }
    }
}
