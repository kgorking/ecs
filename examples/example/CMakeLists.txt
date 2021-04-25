cmake_minimum_required (VERSION 3.15)

add_executable (example "example.cpp")

if (MSVC)
	# generate assembly output
	target_compile_options(example PUBLIC /FAs)
else()
	# haven't gotten it work with clang/gcc yet
	#target_compile_options(example PUBLIC ???)
endif()

target_link_libraries(example ecs)
