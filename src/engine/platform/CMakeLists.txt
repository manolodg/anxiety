set(PAL_SOURCES
	IPlatform.cpp
	PlatformModule.cpp)

if(WIN32)
	list(APPEND PAL_SOURCES
		win32/Win32Platform.cpp
		win32/Win32Window.cpp)
elseif(APPLE)
    enable_language(OBJCXX)                         # requerido para archivos .mm
    list(APPEND PAL_SOURCES
        macos/MacosPlatform.mm
        macos/MacosWindow.mm)
elseif(UNIX)
    # Determina si esta es una compilación para Raspberry Pi ---------------------------------------
    # Activa ANXIETY_PLATFORM_RPI=ON en la línea de comandos de cmake para habilitar el backend
    # DRM/KMS, o se detecta automáticamente en procesadores ARM.
    option(ANXIETY_PLATFORM_RPI "Compila el backend de plataforma DRM/KMS de Raspberry Pi" OFF)

    if(NOT ANXIETY_PLATFORM_RPI AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
        set(ANXIETY_PLATFORM_RPI ON)
        message(STATUS "[anxiety] Procesador ARM detectado — activando el backend de RPi.")
    endif()

    if(ANXIETY_PLATFORM_RPI)
        # Raspberry Pi / DRM+KMS ------------------------------------------------------------------
        add_compile_definitions(ANXIETY_PLATFORM_RPI=1)

        find_package(PkgConfig REQUIRED)
        pkg_check_modules(DRM  REQUIRED libdrm)
        pkg_check_modules(GBM  REQUIRED gbm)

        list(APPEND PAL_SOURCES
            linux/LinuxWindow.cpp                   # Alternativa X11 usada por RpiPlatform
            raspberrypi/RpiPlatform.cpp
            raspberrypi/DrmKmsWindow.cpp)
    else()
        # Linux de escritorio / X11 (+ Wayland opcional) ------------------------------------------
        list(APPEND PAL_SOURCES
            linux/LinuxPlatform.cpp                 # contiene LinuxPlatform + las implementaciones de IPlatformSystem
            linux/LinuxWindow.cpp)

        # Wayland opcional --------------------------------------------------------------------------
        # Experimental y desactivado por defecto: los backends de render (GLX para OpenGL,
        # VK_KHR_xlib_surface para Vulkan) todavía solo saben crear superficies sobre X11. Si se
        # activa esta opción con un compositor Wayland en marcha (WAYLAND_DISPLAY seteado),
        # LinuxPlatform::create_window elegirá WaylandWindow y el render fallará, porque el native
        # handle ya no es una Window de X11. Actívala solo para trabajar en el propio WaylandWindow.
        option(ANXIETY_LINUX_WAYLAND "Compila el backend de ventana Wayland en Linux (experimental, el render aún no lo soporta)" OFF)

        find_package(PkgConfig QUIET)
        if(ANXIETY_LINUX_WAYLAND AND PKG_CONFIG_FOUND)
            pkg_check_modules(WAYLAND_CLIENT    QUIET wayland-client)
            pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
        endif()

        if(ANXIETY_LINUX_WAYLAND AND WAYLAND_CLIENT_FOUND)
            message(STATUS "[anxiety] Cliente de Wayland encontrado — compilando WaylandWindow.")

            # Busca el ejecutable wayland-scanner
            find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

            # Localiza xdg-shell.xml del paquete wayland-protocols
            if(WAYLAND_PROTOCOLS_FOUND)
                pkg_get_variable(WL_PROTOCOLS_DIR wayland-protocols pkgdatadir)
            else()
                set(WL_PROTOCOLS_DIR "/usr/share/wayland-protocols")
            endif()

            set(XDG_SHELL_XML "${WL_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
            if(NOT EXISTS "${XDG_SHELL_XML}")
                message(WARNING "[anxiety] No se encontró xdg-shell.xml en ${XDG_SHELL_XML}. WaylandWindow no se compilará. Instala wayland-protocols.")
            else()
                # Genera la cabecera client-protocol + el código de enlace en el árbol de build
                set(WL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
                file(MAKE_DIRECTORY "${WL_GEN_DIR}")

                set(XDG_SHELL_HDR "${WL_GEN_DIR}/xdg-shell-client-protocol.h")
                set(XDG_SHELL_SRC "${WL_GEN_DIR}/xdg-shell-protocol.c")

                add_custom_command(
                    OUTPUT  "${XDG_SHELL_HDR}"
                    COMMAND "${WAYLAND_SCANNER}" client-header
                            "${XDG_SHELL_XML}" "${XDG_SHELL_HDR}"
                    DEPENDS "${XDG_SHELL_XML}"
                    COMMENT "[anxiety] Generando xdg-shell-client-protocol.h")

                add_custom_command(
                    OUTPUT  "${XDG_SHELL_SRC}"
                    COMMAND "${WAYLAND_SCANNER}" private-code
                            "${XDG_SHELL_XML}" "${XDG_SHELL_SRC}"
                    DEPENDS "${XDG_SHELL_XML}"
                    COMMENT "[anxiety] Generando xdg-shell-protocol.c")

                list(APPEND PAL_SOURCES
                    linux/WaylandWindow.cpp
                    "${XDG_SHELL_SRC}")
                set(ANXIETY_HAS_WAYLAND TRUE)
            endif()
        endif()
    endif()
endif()

add_library(anxiety_platform STATIC ${PAL_SOURCES})
add_library(anxiety::platform ALIAS anxiety_platform)

if(ANXIETY_PLATFORM_RPI)
    target_compile_definitions(anxiety_platform PUBLIC ANXIETY_PLATFORM_RPI=1)
endif()

target_include_directories(anxiety_platform PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_include_directories(anxiety_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win32)
elseif(APPLE)
    target_include_directories(anxiety_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/macos)
elseif(UNIX)
    if(ANXIETY_PLATFORM_RPI)
        target_include_directories(anxiety_platform
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/linux
                ${CMAKE_CURRENT_SOURCE_DIR}/raspberrypi
                ${DRM_INCLUDE_DIRS}
                ${GBM_INCLUDE_DIRS})
    else()
	    target_include_directories(anxiety_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linux)
        if(ANXIETY_HAS_WAYLAND)
            target_include_directories(anxiety_platform PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/wayland-gen")
        endif()
    endif()
endif()

target_compile_features(anxiety_platform PUBLIC cxx_std_20)
set_target_properties(anxiety_platform PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(anxiety_platform PUBLIC anxiety::core)

if(WIN32)
	target_link_libraries(anxiety_platform PRIVATE user32 kernel32)
elseif(APPLE)
    target_link_libraries(anxiety_platform PRIVATE
        "-framework Cocoa"
        "-framework Foundation"
        "-framework IOKit"
        "-framework CoreGraphics")
elseif(UNIX)
    if(ANXIETY_PLATFORM_RPI)
        target_link_libraries(anxiety_platform PRIVATE
            ${DRM_LIBRARIES}
            ${GBM_LIBRARIES})

        # Alternativa X11 en RpiPlatform::createWindow
        find_package(X11 REQUIRED)
        target_include_directories(anxiety_platform SYSTEM PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(anxiety_platform PRIVATE ${X11_LIBRARIES})
    else()
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
endif()