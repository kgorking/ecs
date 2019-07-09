# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("ecs")

# Include sub-projects.
add_subdirectory ("ecs")
add_subdirectory ("examples/example")
add_subdirectory ("examples/parallelism")
add_subdirectory ("examples/finite_state_machine")
add_subdirectory ("examples/mandelbrot")
add_subdirectory ("examples/shared_components")
add_subdirectory ("examples/tagged_components")
