add_library(anxiety_engine STATIC
    CoreModule.cpp
    Engine.cpp
    ModuleGraph.cpp)
add_library(anxiety::engine ALIAS anxiety_engine)

target_include_directories(anxiety_engine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(anxiety_engine INTERFACE cxx_std_20)

target_link_libraries(anxiety_engine PRIVATE 
    anxiety::ecs
    anxiety::logs)