# Editor Somatic (C# / Avalonia) -------------------------------------------------------------------
# Se compila con el SDK de .NET (dotnet) desde CMake para que un único build genere el motor, la DLL
# del bridge y el editor. Es opcional: sin .NET, o con -DANXIETY_BUILD_EDITOR=OFF, se omite y el resto
# del motor se compila igual (Linux / macOS / CI sin SDK de .NET).
# -------------------------------------------------------------------------------------------------
if(NOT ANXIETY_BUILD_EDITOR)
	message(STATUS "[anxiety] Editor Somatic desactivado (ANXIETY_BUILD_EDITOR=OFF).")
	return()
endif()

find_program(DOTNET_CLI NAMES dotnet)
if(NOT DOTNET_CLI)
	message(STATUS "[anxiety] No se encontró el SDK de .NET (dotnet) - el editor Somatic no se compilará.")
	return()
endif()

set(SOMATIC_CSPROJ "${CMAKE_CURRENT_SOURCE_DIR}/Somatic/Somatic.csproj")

# Salida propia del editor, separada de los ejecutables de C++ para no mezclar las DLLs de Avalonia
# con el resto. $<CONFIG> se resuelve al compilar, así que también vale con generadores multi-config
# (Visual Studio), donde CMAKE_BUILD_TYPE está vacío.
set(SOMATIC_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/editor/$<CONFIG>")
set(SOMATIC_DOTNET_CONFIG "$<IF:$<CONFIG:Debug>,Debug,Release>")

# `dotnet build` ya restaura los paquetes NuGet y es incremental, así que basta con este único paso.
add_custom_target(InterfazAvalonia
	ALL
	# La app de C# hace P/Invoke a anxiety_bridge: la DLL (o .so / .dylib) tiene que estar junto al
	# ejecutable del editor. Se le pasa la ruta ya compilada (-p:AnxietyBridgeDll): así el proyecto de C#
	# solo la copia y NO vuelve a invocar a CMake (evita el bucle CMake -> dotnet -> CMake). Cuando se
	# compila el .csproj por su cuenta (Somatic.slnx, IDE), es el propio csproj el que compila el bridge:
	# ver src/editor/Somatic/AnxietyBridge.targets. $<TARGET_FILE:...> resuelve nombre y ruta según SO.
	COMMAND ${DOTNET_CLI} build "${SOMATIC_CSPROJ}"
			--configuration ${SOMATIC_DOTNET_CONFIG}
			--output "${SOMATIC_OUTPUT_DIR}"
			--nologo
			-v:minimal
			"-p:AnxietyBridgeDll=$<TARGET_FILE:anxiety_bridge>"

	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Somatic"
	COMMENT "Compilando el editor Somatic (Avalonia) con .NET CLI..."
	VERBATIM)

# La DLL del bridge debe compilarse ANTES de compilar/copiar el editor (Visual Studio, Rider, CLion...).
add_dependencies(InterfazAvalonia anxiety_bridge)
