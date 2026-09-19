set(RENDERING_SOURCES
	RenderingModule.cpp
	graph/RenderGraph.cpp
	rhi/RHIFactory.cpp
	scene/SceneRenderer.cpp)

# Fuentes del backend específicas de la plataforma -------------------------------------------------
if(WIN32)
	list(APPEND RENDERING_SOURCES
		backend/dx12/DX12CommandBuffer.cpp
		backend/dx12/DX12DescriptorSet.cpp
		backend/dx12/DX12Device.cpp
		backend/dx12/DX12Pipeline.cpp
		backend/dx12/DX12Shader.cpp
		backend/dx12/DX12Swapchain.cpp)
	set(ANXIETY_BACKEND_DX12 ON)

	if(ANXIETY_DX11)
		# DirectX 11 (solo Win32; opcional, para objetivos heredados / de compatibilidad) ---------
		list(APPEND RENDERING_SOURCES
			backend/dx11/DX11DescriptorSet.cpp
			backend/dx11/DX11CommandBuffer.cpp
			backend/dx11/DX11Device.cpp
			backend/dx11/DX11Pipeline.cpp
			backend/dx11/DX11Shader.cpp
			backend/dx11/DX11Swapchain.cpp)
		set(ANXIETY_BACKEND_DX11_ENABLED ON)
		message(STATUS "[anxiety] Backend de DirectX 11 activado.")
	endif()
elseif(APPLE)
	# Metal (solo Apple only) ---------------------------------------------------------------------
	list(APPEND RENDERING_SOURCES
		backend/metal/MetalCommandBuffer.mm
		backend/metal/MetalDescriptorSet.mm
		backend/metal/MetalDevice.mm
		backend/metal/MetalPipeline.mm
		backend/metal/MetalShader.mm
		backend/metal/MetalSwapchain.mm)
	set(ANXIETY_BACKEND_METAL_ENABLED ON)
	message(STATUS "[anxiety] Plataforma Apple - backend de Metal activado.")
endif()

# Vulkan (Linux, Windows optional, macOS via MoltenVK) --------------------------------------------
if(ANXIETY_VULKAN)
	find_package(Vulkan QUIET)
	if(Vulkan_FOUND)
		list(APPEND RENDERING_SOURCES
			backend/vulkan/VulkanCommandBuffer.cpp
			backend/vulkan/VulkanDescriptorSet.cpp
			backend/vulkan/VulkanDevice.cpp
			backend/vulkan/VulkanPipeline.cpp
			backend/vulkan/VulkanShader.cpp
			backend/vulkan/VulkanSwapchain.cpp)
		set(ANXIETY_BACKEND_VULKAN_ENABLED ON)
		message(STATUS "[anxiety] Vulkan encontrado - backend de Vulkan activado.")

		# shaderc — compila nuestros shaders HLSL fuente a SPIR-V en tiempo de ejecución (equivalente
		# a D3DCompile en el backend DX12). Se distribuye con el Vulkan SDK; opcional — sin él,
		# Vulkan::compile_shader_from_source() falla con elegancia y los materiales no cargarán en
		# este backend.
		#
		# pkg-config solo ve shaderc si PKG_CONFIG_PATH incluye el lib/pkgconfig del SDK — cierto en
		# una shell que ha hecho source del setup-env.sh del SDK, pero los IDE (CLion, VS Code, etc.)
		# que lanzan CMake sin heredar ese entorno de shell lo pasarán por alto y desactivarán la
		# compilación de HLSL en silencio. Se recurre a buscar directamente en las rutas de
		# instalación habituales del SDK para que el build no se degrade en silencio solo porque el
		# entorno de proceso del IDE está pelado.
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(SHADERC QUIET IMPORTED_TARGET shaderc)
		endif()

		if(NOT TARGET PkgConfig::SHADERC)
			file(GLOB _anxiety_vulkan_sdk_hints
				"$ENV{VULKAN_SDK}"
				"$ENV{HOME}/vulkan/*/x86_64"
				"$ENV{HOME}/VulkanSDK/*/x86_64"
				"/opt/vulkan*/x86_64")
			find_path(SHADERC_FALLBACK_INCLUDE_DIR shaderc/shaderc.hpp HINTS ${_anxiety_vulkan_sdk_hints} PATH_SUFFIXES include)
			find_library(SHADERC_FALLBACK_LIBRARY NAMES shaderc_combined shaderc_shared shaderc HINTS ${_anxiety_vulkan_sdk_hints} PATH_SUFFIXES lib)
			if(SHADERC_FALLBACK_INCLUDE_DIR AND SHADERC_FALLBACK_LIBRARY)
				add_library(PkgConfig::SHADERC UNKNOWN IMPORTED)
				set_target_properties(PkgConfig::SHADERC PROPERTIES
					IMPORTED_LOCATION             "${SHADERC_FALLBACK_LIBRARY}"
					INTERFACE_INCLUDE_DIRECTORIES "${SHADERC_FALLBACK_INCLUDE_DIR}")
				message(STATUS "[anxiety] shaderc localizado mediante búsqueda directa en el SDK (pkg-config no lo encontró): ${SHADERC_FALLBACK_LIBRARY}")
			endif()
		endif()

		if(TARGET PkgConfig::SHADERC)
			set(ANXIETY_HAVE_SHADERC ON)
			message(STATUS "[anxiety] shaderc encontrado - compilación de shaders HLSL para Vulkan activada.")
		else()
			message(STATUS "[anxiety] shaderc no encontrado - el backend de Vulkan no podrá compilar shaders HLSL en tiempo de ejecución. "
				"Define VULKAN_SDK o PKG_CONFIG_PATH (ver el setup-env.sh del Vulkan SDK) antes de configurar, o pasa "
				"-DSHADERC_FALLBACK_INCLUDE_DIR=... -DSHADERC_FALLBACK_LIBRARY=... explícitamente.")
		endif()
	else()
		message(STATUS "[anxiety] Vulkan no encontrado - backend de Vulkan desactivado.")
	endif()
endif()

# OpenGL 4.x (Windows, Linux, macOS legacy) -------------------------------------------------------
if(ANXIETY_OPENGL)
	find_package(OpenGL QUIET)
	if(OpenGL_FOUND OR OPENGL_FOUND)
		# GLAD — cargador de funciones para el perfil core de OpenGL 4.5.
		# glcorearb.h y wglext.h son cabeceras de Khronos que no están en el Windows SDK,
		# así que GLAD es la forma habitual de obtener las declaraciones de GL 4.x y cargar los punteros a función.
		include(FetchContent)
		FetchContent_Declare(
			glad
			GIT_REPOSITORY https://github.com/Dav1dde/glad.git
			GIT_TAG        v0.1.36
		)
		set(GLAD_PROFILE "core"   CACHE STRING "OpenGL profile"           FORCE)
		set(GLAD_API     "gl=4.5" CACHE STRING "OpenGL API + version"     FORCE)
		set(GLAD_SPEC    "gl"     CACHE STRING "Khronos spec to generate" FORCE)
		# GLAD v0.1.x declara cmake_minimum_required(VERSION 2.8), que CMake 3.27+ rechaza.
		# Esta variable eleva el mínimo a 3.5 para todos los subdirectorios de FetchContent.
		set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE INTERNAL "")
		FetchContent_MakeAvailable(glad)

		list(APPEND RENDERING_SOURCES
			backend/opengl/GLCommandBuffer.cpp
			backend/opengl/GLDescriptorSet.cpp
			backend/opengl/GLDevice.cpp
			backend/opengl/GLPipeline.cpp
			backend/opengl/GLShader.cpp
			backend/opengl/GLSwapchain.cpp)
		set(ANXIETY_BACKEND_OPENGL_ENABLED ON)
		message(STATUS "[anxiety] OpenGL encontrado - backend de OpenGL 4.x activado (cargador GLAD).")
	else()
		message(STATUS "[anxiety] OpenGL no encontrado - backend de OpenGL desactivado.")
	endif()
endif()

# OpenGL ES 3.x (Linux ARM / Android / RPi) -------------------------------------------------------
if(ANXIETY_GLES)
	# EGL es necesario para la gestión de superficies
	find_package(OpenGLES QUIET)
	if(NOT OpenGLES_FOUND)
		# Intenta una búsqueda manual (habitual en Linux embebido)
		find_library(GLES3_LIB NAMES GLESv3 GLESv2 QUIET)
		find_library(EGL_LIB   NAMES EGL           QUIET)
		if(GLES3_LIB AND EGL_LIB)
			set(OpenGLES_FOUND  TRUE)
			set(GLES3_LIBRARIES ${GLES_LIB})
			set(EGL_LIBRARIES   ${EGL_LIB})
		endif()
	endif()

	if(OpenGLES_FOUND OR (GLES3_LIB AND EGL_LIB))
		list(APPEND RENDERING_SOURCES
			backend/gles/GLESDevice.cpp
			backend/gles/GLESSwapchain.cpp)
		# GLESDevice hereda de GLDevice — compila las fuentes base de GL aunque no haya OpenGL de escritorio.
		# GLDevice.cpp / GLCommandBuffer.cpp / GLSwapchain.cpp están protegidos por ANXIETY_BACKEND_OPENGL,
		# así que se define aquí para que esos guards se cumplan.
		if(NOT ANXIETY_BACKEND_OPENGL_ENABLED)
			list(APPEND RENDERING_SOURCES
				backend/opengl/GLCommandBuffer.cpp
				backend/opengl/GLDescriptorSet.cpp
				backend/opengl/GLPipeline.cpp
				backend/opengl/GLShader.cpp
				backend/opengl/GLSwapchain.cpp)
		endif()
		set(ANXIETY_BACKEND_GLES_ENABLED ON)
		message(STATUS "[anxiety] OpenGL ES encontrado - backend de GLES 3.x activado.")
	else()
		message(STATUS "[anxiety] OpenGL ES no encontrado - backend de GLES desactivado.")
	endif()
endif()

# Librerias ---------------------------------------------------------------------------------------
add_library(anxiety_rendering STATIC ${RENDERING_SOURCES})
add_library(anxiety::rendering ALIAS anxiety_rendering)

# Defines de disponibilidad de backends --------------------------------------------------------------
if(ANXIETY_BACKEND_DX12)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_DX12=1)
endif()
if(ANXIETY_BACKEND_DX11_ENABLED)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_DX11=1)
endif()
if(ANXIETY_BACKEND_VULKAN_ENABLED)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_VULKAN=1)
endif()
if(ANXIETY_HAVE_SHADERC)
	target_compile_definitions(anxiety_rendering PRIVATE ANXIETY_HAVE_SHADERC=1)
endif()
if(ANXIETY_BACKEND_METAL_ENABLED)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_METAL=1)
endif()
if(ANXIETY_BACKEND_OPENGL_ENABLED OR ANXIETY_BACKEND_GLES_ENABLED)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_OPENGL=1)
endif()
if(ANXIETY_BACKEND_GLES_ENABLED)
	target_compile_definitions(anxiety_rendering PUBLIC ANXIETY_BACKEND_GLES=1)
endif()

# Rutas de inclusión --------------------------------------------------------------------------------
# PUBLIC: los consumidores de anxiety::rendering pueden hacer #include "rhi/RHI.h",
# "graph/RenderGraph.h" y "RenderingModule.h" directamente.
target_include_directories(anxiety_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(anxiety_rendering PUBLIC cxx_std_20)
set_target_properties(anxiety_rendering PROPERTIES CXX_EXTENSIONS OFF)

# Dependencias --------------------------------------------------------------------------------------
# PUBLIC anxiety::platform - RenderingModule.h incluye PlatformModule.h, así que los consumidores
# necesitan transitivamente la ruta de inclusión de platform.
target_link_libraries(anxiety_rendering
	PUBLIC anxiety::core
	PUBLIC anxiety::platform)

# Librerías de sistema específicas de la plataforma --------------------------------------------------
if(WIN32)
	target_link_libraries(anxiety_rendering PRIVATE d3d12 dxgi d3dcompiler)
	if(ANXIETY_BACKEND_DX11_ENABLED)
		target_link_libraries(anxiety_rendering PRIVATE d3d11)
	endif()
endif()

if(ANXIETY_BACKEND_VULKAN_ENABLED)
	target_link_libraries(anxiety_rendering PRIVATE Vulkan::Vulkan)
	target_include_directories(anxiety_rendering PRIVATE ${Vulkan_INCLUDE_DIRS})

	if(APPLE)
		# VulkanSwapchain crea su VkSurfaceKHR vía VK_EXT_metal_surface, que necesita una
		# CAMetalLayer adjunta al content view de la ventana (AppKit/QuartzCore), igual que el
		# backend nativo de Metal.
		target_link_libraries(anxiety_rendering PRIVATE "-framework AppKit" "-framework QuartzCore")
		set_source_files_properties(
			backend/vulkan/VulkanSwapchain.cpp
			PROPERTIES LANGUAGE OBJCXX COMPILE_OPTIONS "-fobjc-arc")
	endif()

	if(ANXIETY_HAVE_SHADERC)
		target_link_libraries(anxiety_rendering PRIVATE PkgConfig::SHADERC)
	endif()

	if(APPLE)
		# VulkanSwapchain crea su VkSurfaceKHR vía VK_EXT_metal_surface, que necesita una
		# CAMetalLayer adjunta al content view de la ventana (AppKit/QuartzCore), igual que el
		# backend nativo de Metal.
		target_link_libraries(anxiety_rendering PRIVATE "-framework AppKit" "-framework QuartzCore")
		set_source_files_properties(
			backend/vulkan/VulkanSwapchain.cpp
			PROPERTIES LANGUAGE OBJCXX COMPILE_OPTIONS "-fobjc-arc")
	endif()
endif()

if(APPLE AND ANXIETY_BACKEND_METAL_ENABLED)
	target_link_libraries(anxiety_rendering PRIVATE
		"-framework Metal"
		"-framework MetalKit"
		"-framework QuartzCore"
		"-framework AppKit")

	# ARC es necesario para que los casts __bridge_retained / __bridge_transfer del backend de
	# Metal realmente retengan/liberen los objetos Objective-C subyacentes.
	set_source_files_properties(
		backend/metal/MetalCommandBuffer.mm
		backend/metal/MetalDevice.mm
		backend/metal/MetalSwapchain.mm
		PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

if(ANXIETY_BACKEND_OPENGL_ENABLED)
	target_link_libraries(anxiety_rendering PRIVATE OpenGL::GL glad)
	target_compile_definitions(anxiety_rendering PRIVATE ANXIETY_USE_GLAD=1)
	if(WIN32)
		target_link_libraries(anxiety_rendering PRIVATE opengl32)
	elseif(UNIX AND NOT APPLE)
		target_link_libraries(anxiety_rendering PRIVATE GL X11)
	elseif(APPLE)
		# GLDevice/GLSwapchain usan NSOpenGLContext (AppKit) para crear/presentar el contexto GL, y
		# necesitan ARC para que sus casts __bridge_retained / __bridge_transfer retengan/liberen de verdad.
		target_link_libraries(anxiety_rendering PRIVATE "-framework AppKit")
		set_source_files_properties(
			backend/opengl/GLDevice.cpp
			backend/opengl/GLSwapchain.cpp
			PROPERTIES
				LANGUAGE OBJCXX
				COMPILE_OPTIONS "-fobjc-arc"
				COMPILE_DEFINITIONS "GL_SILENCE_DEPRECATION")
	endif()
endif()

if(ANXIETY_BACKEND_GLES_ENABLED)
	# GLAD hace falta incluso para GLES — GLESDevice hereda de GLDevice, que usa las cabeceras/el cargador de GLAD.
	if(NOT ANXIETY_BACKEND_OPENGL_ENABLED)
		include(FetchContent)
		FetchContent_Declare(
			glad
			GIT_REPOSITORY https://github.com/Dav1dde/glad.git
			GIT_TAG        v0.1.36
		)
		set(GLAD_PROFILE "core"   CACHE STRING "OpenGL profile"           FORCE)
		set(GLAD_API     "gl=4.5" CACHE STRING "OpenGL API + version"     FORCE)
		set(GLAD_SPEC    "gl"     CACHE STRING "Khronos spec to generate" FORCE)
		set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE INTERNAL "")
		FetchContent_MakeAvailable(glad)
		target_link_libraries(anxiety_rendering PRIVATE glad)
		target_compile_definitions(anxiety_rendering PRIVATE ANXIETY_USE_GLAD=1)
	endif()

	if(TARGET OpenGLES::OpenGLES)
		target_link_libraries(anxiety_rendering PRIVATE OpenGLES::OpenGLES EGL)
	else()
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(EGL REQUIRED egl)
		target_link_libraries(anxiety_rendering PRIVATE ${EGL_LIBRARIES})
		target_include_directories(anxiety_rendering SYSTEM PRIVATE ${EGL_INCLUDE_DIRS})
		find_library(GLES3_LIB NAMES GLESv3 GLESv2 REQUIRED)
		target_link_libraries(anxiety_rendering PRIVATE ${GLES3_LIB})
	endif()
endif()
