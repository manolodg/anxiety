using Avalonia;

namespace Somatic.Model {
    /// <summary>Estado de la ventana a la salida de la aplicación.</summary>
    public class SomaticWindowState {
#region Propiedades
        /// <summary>Nombre de la pantalla.</summary>
        public string DisplayName { get; set; } = null!;
        /// <summary>Indice de la pantalla.</summary>
        public int Index { get; set; }
        /// <summary>Limites de la ventana.</summary>
        public PixelRect Bounds { get; set; }

        /// <summary>Posición X de la ventana.</summary>
        public double X { get; set; }
        /// <summary>Posición Y de la ventana.</summary>
        public double Y { get; set; }
        /// <summary>Ancho de la ventana.</summary>
        public double Width { get; set; }
        /// <summary>Alto de la ventana.</summary>
        public double Height { get; set; }

        /// <summary>Indica si está maximizada la pantalla.</summary>
        public bool IsMaximized { get; set; }
#endregion
    }
}
