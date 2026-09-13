add_subdirectory(engine)
add_subdirectory(logs)

# anxiety::core - target general del core ---------------------------------------------------------
add_library(anxiety_core INTERFACE)
add_library(anxiety::core ALIAS anxiety_core)

target_link_libraries(anxiety_core INTERFACE
	anxiety::engine
	anxiety::logs)