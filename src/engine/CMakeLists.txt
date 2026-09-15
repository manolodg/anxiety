add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(rendering)

# anxiety::main -----------------------------------------------------------------------------------
#	Con este único target se incluye el motor completo.
#	Cada módulo se referencia mediante su alias ocn namespace para que el grafo de dependencias
#	sea explícito y las herramientas (p. ej. las vistas de proyecto del IDE) lo resuelvan bien.
# -------------------------------------------------------------------------------------------------
add_library(anxiety_main INTERFACE)
add_library(anxiety::main ALIAS anxiety_main)
target_link_libraries(anxiety_main INTERFACE
	anxiety::core
	anxiety::platform
	anxiety::rendering)