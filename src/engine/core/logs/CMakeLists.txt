add_library(anxiety_logs STATIC Logger.cpp)
add_library(anxiety::logs ALIAS anxiety_logs)

target_include_directories(anxiety_logs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(anxiety_logs INTERFACE cxx_std_20)
