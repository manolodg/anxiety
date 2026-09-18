#ifdef ANXIETY_BACKEND_OPENGL

// GLShader es header-only — el constructor/destructor son inlines triviales.
// La compilación real del shader la maneja GLDevice::create_shader(), que llama a
// glCreateShader/glShaderSource/glCompileShader y construye un GLShader a partir
// del objeto de GL resultante.
//
// Esta unidad de traducción existe para que los sistemas de build que requieren un
// emparejamiento 1 a 1 .h/.cpp encuentren el archivo objeto esperado.

#include "GLShader.h"

namespace anxiety::rendering::backend::opengl {
	// Nada que implementar aquí — ver GLShader.h y GLDevice.cpp::create_shader().
} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
