add_library(anxiety_ecs STATIC
	Archetype.cpp
	EcsModule.cpp
	EngineRegistry.cpp
	World.cpp)
add_library(anxiety::ecs ALIAS anxiety_ecs)

target_include_directories(anxiety_ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(anxiety_ecs INTERFACE cxx_std_20)

target_link_libraries(anxiety_ecs PRIVATE
	anxiety::jobs
	anxiety::engine
	anxiety::logs)