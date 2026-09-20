add_executable(anxiety_tests
	tests_main.cpp

    tests_ecs.cpp
    tests_engine.cpp
    tests_jobs.cpp
	tests_logger.cpp
    tests_platform.cpp
    tests_rendering.cpp)

enable_testing()

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.11.0)
FetchContent_MakeAvailable(Catch2)

target_link_libraries(anxiety_tests PRIVATE
    anxiety::main
    Catch2::Catch2WithMain)
target_compile_features(anxiety_tests PRIVATE cxx_std_20)

set_target_properties(anxiety_tests PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME    "anxiety_tests")

include(CTest)

# Registrar en CTest
add_test(NAME anxiety_tests COMMAND anxiety_tests)