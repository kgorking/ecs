cmake_minimum_required (VERSION 3.15)

add_executable (example "example.cpp")

if (MSVC)
	# generate assembly output
	#target_compile_options(example PUBLIC -masm=intel)
	target_compile_options(example PUBLIC /Fa)
else()
	# haven't gotten it work with clang/gcc yet
endif()

target_link_libraries(example ecs)
