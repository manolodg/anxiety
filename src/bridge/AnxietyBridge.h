#pragma once

// AnxietyBridge -------------------------------------------------------------------------------
// ABI en C mínima y estable para hospedar el motor desde un llamador ajeno a C++ (hoy: el editor
// Somatic vía P/Invoke). Handle opaco, sin STL ni tipos de C++ en la frontera.
//
// Ciclo de vida:
//   anxiety_bridge_create()  -> arranca un Engine en su propio hilo (bloqueado en run()) con un
//                               RenderingModule en modo ventana embebida (backend por defecto de la plataforma).
//   anxiety_bridge_destroy() -> detiene ese Engine y espera a que su hilo termine.
//
// Ventana (una sola a la vez por ahora):
//   anxiety_bridge_attach_window()  -> el motor empieza a dibujar en una ventana nativa que es propiedad
//                                      del llamador (HWND / Window X11 / NSView).
//   anxiety_bridge_resize()         -> no bloqueante; se aplica al inicio del siguiente fotograma.
//   anxiety_bridge_detach_window()  -> síncrona: al volver, el motor ya no usa la ventana. Hay que
//                                      llamarla ANTES de destruir la ventana nativa y antes de anxiety_bridge_destroy().
// Todas son seguras de llamar desde un hilo distinto al del motor (p. ej. el hilo de UI).
// -------------------------------------------------------------------------------------------

#include <stdint.h>

#if defined(_WIN32)
#if defined(ANXIETY_BRIDGE_EXPORTS)
#define ANXIETY_BRIDGE_API extern "C" __declspec(dllexport)
#else
#define ANXIETY_BRIDGE_API extern "C" __declspec(dllimport)
#endif
#else
#define ANXIETY_BRIDGE_API extern "C" __attribute__((visibility("default")))
#endif

typedef struct AnxietyEngineHandle_* AnxietyEngineHandle;

// Crea y arranca un Engine headless en un hilo propio. Devuelve NULL si init() falla.
ANXIETY_BRIDGE_API AnxietyEngineHandle anxiety_bridge_create(void);

// Detiene el Engine y une su hilo. NULL o doble destroy son no-ops seguros.
ANXIETY_BRIDGE_API void anxiety_bridge_destroy(AnxietyEngineHandle handle);

// Adjunta la ventana nativa. Devuelve 1 si el swapchain se creó, 0 si falla (handle o ventana nulos,
// tamaño 0, o el backend no pudo crear el swapchain).
ANXIETY_BRIDGE_API int anxiety_bridge_attach_window(AnxietyEngineHandle handle, void* native_window, uint32_t width, uint32_t height);

// Nuevo tamaño en píxeles físicos. Tamaño 0 (ventana minimizada) se ignora.
ANXIETY_BRIDGE_API void anxiety_bridge_resize(AnxietyEngineHandle handle, uint32_t width, uint32_t height);

// Suelta la ventana adjuntada. No-op si no hay ninguna.
ANXIETY_BRIDGE_API void anxiety_bridge_detach_window(AnxietyEngineHandle handle);

// Nombre del backend gráfico en uso ("DirectX 12", "Vulkan", "Metal"...). La cadena es propiedad del
// handle y vive hasta anxiety_bridge_destroy(); NO hay que liberarla. NULL si handle es NULL.
ANXIETY_BRIDGE_API const char* anxiety_bridge_backend_name(AnxietyEngineHandle handle);
