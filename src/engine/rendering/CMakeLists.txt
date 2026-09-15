set(RENDERING_SOURCES
	RenderingModule.cpp
	graph/RenderGraph.cpp)

# Fuentes del backend específicas de la plataforma -------------------------------------------------
if(WIN32)
	list(APPEND RENDERING_SOURCES
		backend/dx12/DX12Device.cpp
		backend/dx12/DX12CommandBuffer.cpp
		backend/dx12/DX12Swapchain.cpp)
elseif(UNIX AND NOT APPLE)
	# ...
endif()

add_library(anxiety_rendering STATIC ${RENDERING_SOURCES})
add_library(anxiety::rendering ALIAS anxiety_rendering)

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
elseif(UNIX AND NOT APPLE)
	# find_package(Vulkan REQUIRED)
	# target_link_libraries(anxiety_rendering PRIVATE Vulkan::Vulkan)
endif()