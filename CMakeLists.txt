cmake_minimum_required (VERSION 3.25)
project(anxiety
	VERSION     0.1.0
	DESCRIPTION "GameEngine AAA modular"
	LANGUAGES   CXX)

# Habilite Recarga activa para los compiladores de MSVC si se admiten.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Opciones ========================================================================================
# =================================================================================================

# Estándar global C++ -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Tipo de construcción por defecto ----------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "[anxiety] No se ha indicado el tipo de build - por defecto Debug")
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Directorios de salida ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVe_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Flags del compilador ----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /W4                     # Nivel de warnings 4
        /WX                     # Warnings como errores
        /MP                     # Compilación multiprocesador
        /utf-8                  # Juego de caracteres UTF-8 en fuente y en ejecución
        /Zc:__cplusplus         # Reportar el valor correcto de __cplusplus
        /Zc:preprocessor        # Preprocesador conforme al estándar
        # Suprimir warnings molestos de MSVC que son falsos positivos en código C++20
        /wd4251                 # Interfaz DLL
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    # Configuración de Debug - un flag por expresión de generador
    add_compile_options(
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/RTC1>
    )
    # Configuración de Release
    add_compile_options(
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/DNDEBUG>
    )
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
    )
endif()

# Detección de plataforma -------------------------------------------------------------------------
if(WIN32)
    add_compile_definitions(ANXIETY_PLATFORM_WINDOWS=1)
elseif(APPLE)
    add_compile_definitions(ANXIETY_PLATFORM_MACOS=1)
elseif(UNIX)
    add_compile_definitions(ANXIETY_PLATFORM_LINUX=1)
endif()

# Subdirectorios ==================================================================================
add_subdirectory(src)                   # Este es el motor de juego Anxiety

add_subdirectory(sandbox)               # Aplicación de pruebas del concepto actual en desarrollo

add_subdirectory(tests)                 # Añade los tests que se hacen para comprobar el sistema
# =================================================================================================