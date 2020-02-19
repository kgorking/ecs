# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project (ecs CXX)

# Require c++17
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED on)

# Include google benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)
add_subdirectory ("benchmark/gbench")

# Include GSL
add_subdirectory ("GSL")

# Include sub-projects.
add_subdirectory ("include")
add_subdirectory ("benchmark")
add_subdirectory ("examples/example")
add_subdirectory ("examples/entt_example")
add_subdirectory ("examples/parallelism")
add_subdirectory ("examples/finite_state_machine")
add_subdirectory ("examples/mandelbrot")
add_subdirectory ("examples/shared_components")
add_subdirectory ("examples/tagged_components")

enable_testing()
add_subdirectory ("unittest")
