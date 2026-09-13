add_executable(sandbox
	main.cpp)

target_link_libraries(sandbox PRIVATE anxiety::main)
target_compile_features(sandbox PRIVATE cxx_std_20)

set_target_properties(sandbox PROPERTIES
	CXX_EXTENSIONS OFF
	OUTPUT_NAME    "sandbox")