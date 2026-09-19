add_library(anxiety_jobs STATIC
    JobSystem.cpp)
add_library(anxiety::jobs ALIAS anxiety_jobs)

target_include_directories(anxiety_jobs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(anxiety_jobs INTERFACE cxx_std_20)
set_target_properties(anxiety_jobs PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(anxiety_jobs PUBLIC
    anxiety::core)