# anxiety_bridge -------------------------------------------------------------------------------
# ABI en C que expone el motor a llamadores ajenos a C++ (P/Invoke desde el editor Somatic).
# A diferencia del resto de módulos del motor (STATIC/INTERFACE), esta es una librería SHARED:
# es precisamente el artefacto que cruza la frontera del proceso/lenguaje.
# -------------------------------------------------------------------------------------------

add_library(anxiety_bridge SHARED AnxietyBridge.cpp "AnxietyBridge.h")
add_library(anxiety::bridge ALIAS anxiety_bridge)

target_include_directories(anxiety_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(anxiety_bridge PRIVATE cxx_std_20)
set_target_properties(anxiety_bridge PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(anxiety_bridge PRIVATE anxiety::main)

# Activa __declspec(dllexport) en AnxietyBridge.h solo al compilar esta librería;
# cualquier consumidor externo en C++ vería dllimport.
target_compile_definitions(anxiety_bridge PRIVATE ANXIETY_BRIDGE_EXPORTS)
