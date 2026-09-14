set(PAL_SOURCES
	IPlatform.cpp
	PlatformModule.cpp)

if(WIN32)
	list(APPEND PAL_SOURCES
		win32/Win32Platform.cpp
		win32/Win32Window.cpp)
elseif(UNIX AND NOT APPLE)
	# Escritorio Linux / X11 (+ Wayland opcional) -------------------------------------------------
	list(APPEND PAL_SOURCES
		linux/LinuxPlatform.cpp
		linux/LinuxWindow.cpp)

	# Wayland opcional ----------------------------------------------------------------------------
	# sudo apt install libwayland-dev wayland-protocols
	find_package(PkgConfig QUIET)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(WAYLAND_CLIENT    QUIET wayland-client)
		pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
	endif()

	if(WAYLAND_CLIENT_FOUND)
		message(STATUS "[anxiety] Encontrado cliente Wayland - construyendo WaylandWindow.")

		# Encontrar ejecutable wayland-scanner
		find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
		# Localizar xdg-shell.xml en el paquete wayland-protocols
		if(WAYLAND_PROTOCOLS_FOUND)
			pkg_get_variable(WL_PROTOCOLS_DIR wayland-protocols pkgdatadir)
		else()
			set(WL_PROTOCOLS_DIR "/usr/share/wayland-protocols")
		endif()

		set(XDG_SHELL_XML "${WL_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
		if(NOT EXISTS "${XDG_SHELL_XML}")
			message(WARNING "[anxiety] xdg-shell.xml no encontrado en ${XDG_SHELL_XML}. WaylandWindow no se compilará. Instala wayland-protocols.")
		else()
			# Genera la cabecera client-protocol + fuente glue en el arbol de construcción
			set(WL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
			file(MAKE_DIRECTORY "${WL_GEN_DIR}")

			set(XDG_SHELL_HDR "${WL_GEN_DIR}/xdg-shell-client-protocol.h")
			set(XDG_SHELL_SRC "${WL_GEN_DIR}/xdg-shell-protocol.c")

			add_custom_command(
					OUTPUT  "${XDG_SHELL_HDR}"
					COMMAND "${WAYLAND_SCANNER}" client-header
					"${XDG_SHELL_XML}" "${XDG_SHELL_HDR}"
					DEPENDS "${XDG_SHELL_XML}"
					COMMENT "[anxiety] Generating xdg-shell-client-protocol.h")

			add_custom_command(
					OUTPUT  "${XDG_SHELL_SRC}"
					COMMAND "${WAYLAND_SCANNER}" private-code
					"${XDG_SHELL_XML}" "${XDG_SHELL_SRC}"
					DEPENDS "${XDG_SHELL_XML}"
					COMMENT "[anxiety] Generating xdg-shell-protocol.c")

			list(APPEND PAL_SOURCES
					linux/WaylandWindow.cpp
					"${XDG_SHELL_SRC}"
					"${XDG_SHELL_HDR}")
			set(ANXIETY_HAS_WAYLAND TRUE)
		endif()
	endif()
endif()

add_library(anxiety_platform STATIC ${PAL_SOURCES})
add_library(anxiety::platform ALIAS anxiety_platform)

target_include_directories(anxiety_platform
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win32)

target_compile_features(anxiety_platform PUBLIC cxx_std_20)
target_link_libraries(anxiety_platform PUBLIC anxiety::core)
set_target_properties(anxiety_platform PROPERTIES CXX_EXTENSIONS OFF)

if(WIN32)
	target_link_libraries(anxiety_platform PRIVATE user32 kernel32)
elseif(UNIX AND NOT APPLE)
	# Requiere el paquete de desarrollo X11 (libx11-dev en Debian/Ubuntu). X11_FOUND se da con find_package;
	# las cabeceras se tratan como SYSTEM para suprimir cualquier aviso redundante desde el mismo Xlib.
	find_package(X11 REQUIRED)
	target_include_directories(anxiety_platform SYSTEM PRIVATE ${X11_INCLUDE_DIR})
	target_link_libraries(anxiety_platform PRIVATE ${X11_LIBRARIES})

	if(ANXIETY_HAS_WAYLAND)
		target_compile_definitions(anxiety_platform PRIVATE ANXIETY_HAS_WAYLAND)
		target_include_directories(anxiety_platform SYSTEM PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS} "${WL_GEN_DIR}")
		target_link_libraries(anxiety_platform PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
	endif()
endif()