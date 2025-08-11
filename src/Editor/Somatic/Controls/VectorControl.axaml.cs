using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Somatic.Model;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.RegularExpressions;

namespace Somatic.Controls {
    /// <summary>Control que se encarga de gestionar vectores.</summary>
    public partial class VectorControl : UserControl {
#region Campos Globales
        /// <remarks>
        /// Como los números dependen totalmente del idioma en el que está la aplicación, se obtienen las constantes
        /// necesarias para poder gestionar de forma independiente el idioma en el que se encuentre la aplicación.
        /// </remarks>
        private static readonly CultureInfo CurrentCulture = CultureInfo.CurrentCulture;
        private static readonly string DecimalSeparator = CurrentCulture.NumberFormat.NumberDecimalSeparator;
        private static readonly string NegativeSign = CurrentCulture.NumberFormat.NegativeSign;

        private static readonly Regex NumberRegex = new Regex($@"^{Regex.Escape(NegativeSign)}?\d*{Regex.Escape(DecimalSeparator)}?\d*$", RegexOptions.Compiled);
#endregion

#region Propiedades Avalonia
        /// <summary>Sensibilidad que se le proporciona al arrastre</summary>
        public static readonly StyledProperty<float> DragSensitivityProperty = AvaloniaProperty.Register<VectorControl, float>(nameof(DragSensitivity), 0.01f);
        /// <summary>Sensibilidad que se le proporciona al movimiento de rueda.</summary>
        public static readonly StyledProperty<float> WheelStepProperty = AvaloniaProperty.Register<VectorControl, float>(nameof(WheelStep), 0.01f);

        /// <summary>Valor del vector contenido.</summary>
        public static readonly StyledProperty<VectorEditor> VectorProperty = AvaloniaProperty.Register<VectorControl, VectorEditor>(nameof(Vector), new VectorEditor(0, 0, 0));
        /// <summary>Indica si es un control solo de lectura.</summary>
        public static readonly StyledProperty<bool> IsReadOnlyProperty = AvaloniaProperty.Register<VectorControl, bool>(nameof(IsReadOnly), false);

        /// <summary>Texto usado para manejar X</summary>
        public static readonly StyledProperty<string> XTextProperty = AvaloniaProperty.Register<VectorControl, string>(nameof(XText), $"0{DecimalSeparator}000");
        /// <summary>Texto usado para manejar Y</summary>
        public static readonly StyledProperty<string> YTextProperty = AvaloniaProperty.Register<VectorControl, string>(nameof(YText), $"0{DecimalSeparator}000");
        /// <summary>Texto usado para manejar Z</summary>
        public static readonly StyledProperty<string> ZTextProperty = AvaloniaProperty.Register<VectorControl, string>(nameof(ZText), $"0{DecimalSeparator}000");
#endregion

#region Propiedades
        /// <summary>Sensibilidad que se le proporciona al arrastre</summary>
        public float DragSensitivity {
            get => GetValue(DragSensitivityProperty);
            set => SetValue(DragSensitivityProperty, Math.Max(0.001f, value));
        }
        /// <summary>Sensibilidad que se le proporciona al movimiento de rueda.</summary>
        public float WheelStep {
            get => GetValue(WheelStepProperty);
            set => SetValue(WheelStepProperty, Math.Max(0.001f, value));
        }

        /// <summary>Valor del vector contenido.</summary>
        public VectorEditor Vector {
            get => GetValue(VectorProperty);
            set => SetValue(VectorProperty, value);
        }
        /// <summary>Indica si es un control solo de lectura.</summary>
        public bool IsReadOnly {
            get => GetValue(IsReadOnlyProperty);
            set => SetValue(IsReadOnlyProperty, value);
        }

        /// <summary>Texto usado para manejar X</summary>
        public string XText {
            get => GetValue(XTextProperty);
            set => SetValue(XTextProperty, value);
        }
        /// <summary>Texto usado para manejar Y</summary>
        public string YText {
            get => GetValue(YTextProperty);
            set => SetValue(YTextProperty, value);
        }
        /// <summary>Texto usado para manejar Z</summary>
        public string ZText {
            get => GetValue(ZTextProperty);
            set => SetValue(ZTextProperty, value);
        }
#endregion

#region Campos
        private TextBox? _draggingTextBox;
        private Point _lastPointerPosition;
        private bool _isDragging;
        private float _dragStartValue;
#endregion

#region Constructores
        /// <summary>Crea una instancia de la clase <see cref="VectorControl"/>.</summary>
        public VectorControl() {
            InitializeComponent();
            SetupTextBoxEvents();
        }
#endregion

#region Métodos
    #region Eventos Pantalla
        /// <summary>Se produce cuando se ha de notificar un cambio a la propiedad.</summary>
        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) {
            if (change.Property == XTextProperty) {
                Vector.X = string.IsNullOrEmpty(XText) ? 0 : float.Parse(XText, NumberStyles.Float, CurrentCulture);
            } else if (change.Property == YTextProperty) {
                Vector.Y = string.IsNullOrEmpty(YText) ? 0 : float.Parse(YText, NumberStyles.Float, CurrentCulture);
            } else if (change.Property == ZTextProperty) {
                Vector.Z = string.IsNullOrEmpty(ZText) ? 0 : float.Parse(ZText, NumberStyles.Float, CurrentCulture);
            } else if (change.Property == VectorProperty) {
                AssignVector((VectorEditor)change.NewValue!);
            }

            base.OnPropertyChanged(change);
        }

        /// <summary>Controlamos la inserción de valores mediante el teclado.</summary>
        private void OnTextBoxKeyDown(object? sender, KeyEventArgs e) {
            // Dejamos que se procese cualquier tecla de control que nos interese
            if (e.Key == Key.Back || e.Key == Key.Delete || e.Key == Key.Left || e.Key == Key.Right || e.Key == Key.Home || e.Key == Key.End || e.Key == Key.Tab || e.Key == Key.Enter || (e.KeyModifiers.HasFlag(KeyModifiers.Control) && (e.Key == Key.A || e.Key == Key.C || e.Key == Key.V || e.Key == Key.X))) return;
            // Dejamos que se procese cualquier tecla numerica tanto del teclado como del teclado numerico 
            if ((e.Key >= Key.D0 && e.Key <= Key.D9) || (e.Key >= Key.NumPad0 && e.Key <= Key.NumPad9)) return;
            // Dejamos que se procese la tecla correspondiente al separador de decimales (por cultura)
            if (IsDecimalSeparatorKey(e.Key)) return;
            // Dejamos que se procese la tecla correspondiente al signo negativo (por cultura)
            if (IsNegativeSignKey(e.Key)) return;

            // Si se pulsa tecla arriba o abajo se procesa de forma especial
            if (e.Key == Key.Up || e.Key == Key.Down) HandleArrowKeyIncrement(sender as TextBox, e.Key == Key.Up);

            e.Handled = true;
        }
        /// <summary>Comprobación de que el texto métido es válido.</summary>
        private void OnTextBoxTextInput(object? sender, TextInputEventArgs e) {
            if (sender is not TextBox textBox || string.IsNullOrEmpty(e.Text)) return;

            foreach (char c in e.Text) {
                if (!IsValidCharacter(c)) {
                    e.Handled = true;
                    return;
                }
            }

            string currentText = textBox.Text ?? "";
            int caretIndex = textBox.CaretIndex;
            string newText = currentText.Insert(caretIndex, e.Text);

            if (!ValidateNumericInput(newText)) e.Handled = true;
        }
        /// <summary>Al obtener el foco se selecciona el contenido entero.</summary>
        private void OnTextBoxGotFocus(object? sender, GotFocusEventArgs e) {
            if (sender is TextBox textBox) textBox.SelectAll();
        }
        /// <summary>Control del movimiento de la rueda del ratón.</summary>
        private void OnTextBoxPointerWheelChanged(object? sender, PointerWheelEventArgs e) {
            if (sender is not TextBox textBox) return;
            if (!textBox.IsFocused) return;

            double delta = e.Delta.Y;
            float increment = delta > 0 ? WheelStep : -WheelStep;

            if (e.KeyModifiers.HasFlag(KeyModifiers.Shift)) {
                increment *= 10;
            } else if (e.KeyModifiers.HasFlag(KeyModifiers.Control)) {
                increment *= 0.1f;
            }

            IncrementTextBoxValue(textBox, increment);
            e.Handled = true;
        }
        /// <summary>Controlamos el arrastre en el caso del ratón con botón medio pulsado.</summary>
        private void OnTextBoxPointerPressed(object? sender, PointerPressedEventArgs e) {
            if (sender is not TextBox textBox || !e.GetCurrentPoint(textBox).Properties.IsMiddleButtonPressed) return;

            _draggingTextBox = textBox;
            _lastPointerPosition = e.GetCurrentPoint(this).Position;
            _isDragging = false;
            _dragStartValue = GetTextBoxFloatValue(textBox);

            textBox.Cursor = new Cursor(StandardCursorType.SizeNorthSouth);
            e.Pointer.Capture(textBox);

            e.Handled = true;
        }
        /// <summary>Realizamos el cálculo del movimiento del ratón al arrastrar.</summary>
        private void OnTextBoxPointerMoved(object? sender, PointerEventArgs e) {
            if (_draggingTextBox == null || sender != _draggingTextBox) return;

            Point currenPosition = e.GetCurrentPoint(this).Position;
            float deltaY = (float)(_lastPointerPosition.Y - currenPosition.Y);

            if (!_isDragging && Math.Abs(deltaY) > 2) _isDragging = true;
            if (_isDragging) {
                float increment = deltaY * DragSensitivity;

                if (e.KeyModifiers.HasFlag(KeyModifiers.Shift)) {
                    increment *= 10;
                } else if (e.KeyModifiers.HasFlag(KeyModifiers.Control)) {
                    increment *= 0.1f;
                }

                float newValue = _dragStartValue + increment;
                SetTextBoxFloatValue(_draggingTextBox, newValue);
            }

            e.Handled = true;
        }
        /// <summary>Liberamos el arrastre una vez se suelta el botón central.</summary>
        private void OnTextBoxPointerReleased(object? sender, PointerReleasedEventArgs e) {
            e.Pointer.Capture(null);

            EndDragging(); 
        }
        /// <summary>Finalizamos el arrastre.</summary>
        private void OnTextBoxPointerCaptureLost(object? sender, PointerCaptureLostEventArgs e) => EndDragging();
    #endregion

    #region Métodos
        /// <summary>Asignación de la propiedad de referencia.</summary>
        public void AssignVector(VectorEditor vector) {
            Vector = vector;

            SetValue(XTextProperty, vector.X.ToString("F3"));
            SetValue(YTextProperty, vector.Y.ToString("F3"));
            SetValue(ZTextProperty, vector.Z.ToString("F3"));
        }

        /// <summary>Configura el comportamiento de todos los elementos de la pantalla.</summary>
        private void SetupTextBoxEvents() {
            IEnumerable<TextBox>? textBoxes = this.FindControl<Grid>("ComponentsPanel")?.Children.OfType<TextBox>().Where(tb => tb.Classes.Contains("vector-component"));
            if (textBoxes != null) {
                foreach (TextBox textBox in textBoxes) {
                    textBox.KeyDown += OnTextBoxKeyDown;
                    textBox.TextInput += OnTextBoxTextInput;
                    textBox.GotFocus += OnTextBoxGotFocus;

                    textBox.PointerWheelChanged += OnTextBoxPointerWheelChanged;
                    textBox.AddHandler(InputElement.PointerPressedEvent, OnTextBoxPointerPressed, RoutingStrategies.Tunnel);
                    textBox.PointerMoved += OnTextBoxPointerMoved;
                    textBox.PointerReleased += OnTextBoxPointerReleased;
                    textBox.PointerCaptureLost += OnTextBoxPointerCaptureLost;
                }
            }
        }
        /// <summary>Incrementa el valor según la dirección de la flecha (emulando la rueda del ratón).</summary>
        private void HandleArrowKeyIncrement(TextBox? textBox, bool increment) {
            if (textBox == null) return;

            float step = increment ? WheelStep : -WheelStep;
            IncrementTextBoxValue(textBox, step);
        }
        /// <summary>Se realiza el cálculo del cambio de valor en el TextBox.</summary>
        private void IncrementTextBoxValue(TextBox textBox, float increment) {
            float currentValue = GetTextBoxFloatValue(textBox);
            float newValue = currentValue + increment;
            SetTextBoxFloatValue(textBox, newValue);
        }
        /// <summary>Se toma el valor del TextBox y se convierte a float</summary>
        private float GetTextBoxFloatValue(TextBox textBox) {
            string text = textBox.Text ?? "0";
            return float.TryParse(text, NumberStyles.Float, CurrentCulture, out float result) ? result : 0f;
        }
        /// <summary>Se asigna el valor que corresponda</summary>
        private void SetTextBoxFloatValue(TextBox textBox, float value) {
            string formattedValue = value.ToString("F3", CurrentCulture).TrimEnd('0').TrimEnd(DecimalSeparator.ToCharArray());
            if (formattedValue == "" || formattedValue == NegativeSign) formattedValue = "0";

            //textBox.Text = formattedValue;
            if (textBox.Name == "XColumn") SetValue(XTextProperty, formattedValue);
            if (textBox.Name == "YColumn") SetValue(YTextProperty, formattedValue);
            if (textBox.Name == "ZColumn") SetValue(ZTextProperty, formattedValue);
        }
        /// <summary>Liberamos el arrastre</summary>
        private void EndDragging() {
            if (_draggingTextBox != null) {
                _draggingTextBox.Cursor = Cursor.Default;
                
                _draggingTextBox = null;
            }

            _isDragging = false;
        }

        private bool ValidateNumericInput(string input) => IsValidNumber(input);
        private bool IsValidCharacter(char c) => char.IsDigit(c) || c.ToString() == DecimalSeparator || c.ToString() == NegativeSign;

        /// <summary>Comprobación del seprador.</summary>
        private static bool IsDecimalSeparatorKey(Key key) => DecimalSeparator switch {
            "." => key == Key.OemPeriod || key == Key.Decimal,
            "," => key == Key.OemComma || key == Key.Decimal,
            _   => key == Key.OemPeriod || key == Key.Decimal || key == Key.OemComma
        };
        /// <summary>Comprobación del signo negativo.</summary>
        private static bool IsNegativeSignKey(Key key) => key == Key.OemMinus || key == Key.Subtract;
        /// <summary>Comprobación de validez del número.</summary>
        private static bool IsValidNumber(string text) {
            if (string.IsNullOrEmpty(text)) return false;
            if (text == NegativeSign || text == DecimalSeparator || text == NegativeSign + DecimalSeparator) return true;
            return NumberRegex.IsMatch(text);
        }
    #endregion
#endregion
    }
}
