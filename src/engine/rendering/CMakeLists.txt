set(RENDERING_SOURCES
	RenderingModule.cpp
	graph/RenderGraph.cpp
	rhi/RHIFactory.cpp)

# Fuentes del backend específicas de la plataforma -------------------------------------------------
if(WIN32)
	list(APPEND RENDERING_SOURCES
		backend/dx12/DX12Device.cpp
		backend/dx12/DX12CommandBuffer.cpp
		backend/dx12/DX12Swapchain.cpp)
	set(ANXIETY_BACKEND_DX12 ON)

	if(ANXIETY_DX11)
		# DirectX 11 (solo Win32; opcional, para objetivos heredados / de compatibilidad) ---------
		list(APPEND RENDERING_SOURCES
			backend/dx11/DX11CommandBuffer.cpp
			backend/dx11/DX11Device.cpp
			backend/dx11/DX11Swapchain.cpp)
		set(ANXIETY_BACKEND_DX11_ENABLED ON)
		message(STATUS "[anxiety] Backend de DirectX 11 activado.")
	endif()
elseif(UNIX AND NOT APPLE)
	# ...
endif()

# Vulkan (Linux, Windows optional, macOS via MoltenVK) --------------------------------------------
if(ANXIETY_VULKAN)
	find_package(Vulkan QUIET)
	if(Vulkan_FOUND)
		list(APPEND RENDERING_SOURCES
			backend/vulkan/VulkanCommandBuffer.cpp
			backend/vulkan/VulkanDevice.cpp
			backend/vulkan/VulkanSwapchain.cpp)
		set(ANXIETY_BACKEND_VULKAN_ENABLED ON)
		message(STATUS "[anxiety] Vulkan encontrado - backend de Vulkan activado.")
	else()
		message(STATUS "[anxiety] Vulkan no encontrado - backend de Vulkan desactivado.")
	endif()
endif()

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
	target_link_libraries(anxiety_rendering PRIVATE d3d12 dxgi)
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
endif()
