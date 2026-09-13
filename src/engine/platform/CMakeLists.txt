set(PAL_SOURCES
	IPlatform.cpp
	PlatformModule.cpp)

if(WIN32)
	list(APPEND PAL_SOURCES
		win32/Win32Platform.cpp
		win32/Win32Window.cpp)
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
endif()