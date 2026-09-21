namespace Somatic.Infrastructure.Configuration {
    public sealed class EditorOptions {
        public const string SectionName = "Editor";

        public ThemeOptions  Theme    { get; set; } = new();
        public string        Language { get; set; } = "en";
        public WindowOptions Window   { get; set; } = new();
    }

    public sealed class ThemeOptions {
        public string Default { get; set; } = "Dark";
    }

    public sealed class WindowOptions {
        public double Width { get; set; }     = 1440;
        public double Height { get; set; }    =  900;
        public double MinWidth { get; set; }  = 1024;
        public double MinHeight { get; set; } =  640;
    }}
