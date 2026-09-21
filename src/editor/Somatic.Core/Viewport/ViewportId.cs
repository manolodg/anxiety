namespace Somatic.Core.Viewport {
    public readonly record struct ViewportId(string Value) {
        public override string ToString() => Value;
    }
}
