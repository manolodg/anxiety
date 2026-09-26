using System.Globalization;

namespace Somatic.Core.Scene {
    /// <summary>Color RGBA con componentes en [0, 1].</summary>
    public readonly record struct ColorRgba(float R, float G, float B, float A = 1.0f) {
        public static ColorRgba White => new(1.0f, 1.0f, 1.0f);

        /// <summary>Formato <c>#RRGGBB</c> (o <c>#RRGGBBAA</c> si no es opaco).</summary>
        public string ToHex() {
            string rgb = $"#{ToByte(R):X2}{ToByte(G):X2}{ToByte(B):X2}";
            return A >= 1.0f ? rgb : rgb + ToByte(A).ToString("X2");
        }

        public static bool TryParseHex(string? text, out ColorRgba color) {
            color = default;
            if (string.IsNullOrWhiteSpace(text)) return false;

            ReadOnlySpan<char> hex = text.AsSpan().Trim().TrimStart('#');
            if (hex.Length != 6 && hex.Length != 8) return false;
            if (!uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint value)) return false;

            if (hex.Length == 6) value = (value << 8) | 0xFF;
            color = new ColorRgba(((value >> 24) & 0xFF) / 255.0f, ((value >> 16) & 0xFF) / 255.0f, ((value >> 8) & 0xFF) / 255.0f, (value & 0xFF) / 255.0f);
            return true;
        }

        private static byte ToByte(float channel) => (byte)MathF.Round(Math.Clamp(channel, 0.0f, 1.0f) * 255.0f);
    }
}
