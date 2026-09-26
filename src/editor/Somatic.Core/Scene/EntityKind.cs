namespace Somatic.Core.Scene {
    /// <summary>
    /// Tipo de entidad. Decide el icono con el que se muestra en la jerarquía y los componentes con los que
    /// nace (ver <see cref="EntityFactory"/>); después la entidad puede ganar o perder componentes libremente.
    /// </summary>
    public enum EntityKind {
        Empty,
        Group,
        Camera,
        Light,
        Mesh
    }
}
