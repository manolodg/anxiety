using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Dock.Model.Core;
using Somatic.ViewModels;
using System;

namespace Somatic {
    /// <summary>Herramienta que hace la redirección entre el ViewModel y el View.</summary>
    public class ViewLocator : IDataTemplate {
        /// <summary>Realiza la construcción de la vista mediante el ViewModel.</summary>
        /// <param name="param">ViewModel para realizar la construcción.</param>
        /// <returns>Control vinculado al ViewModel.</returns>
        public Control? Build(object? param) {
            if (param is null) return null;

            string name = param.GetType().FullName!.Replace("ViewModel", "View", StringComparison.InvariantCulture);
            Type? type = Type.GetType(name);

            if (type == null) return null;

            Control control = (Control)Activator.CreateInstance(type)!;
            control.DataContext = param;
            return control;
        }

        /// <summary>Comprueba si el objeto se ha de construir.</summary>
        /// <param name="data">Objeto a comprobar.</param>
        /// <returns>Indica si se ha de construir o no.</returns>
        public bool Match(object? data) => data is ViewModelBase || data is IDockable;
    }
}
