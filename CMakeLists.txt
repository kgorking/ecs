# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("ecs")

enable_testing()

# GSL throws an exception instead of calling std::terminate
add_compile_definitions(GSL_THROW_ON_CONTRACT_VIOLATION)
set( CMAKE_CXX_STANDARD 17 )
set( CXX_STANDARD_REQUIRED on)

# Include google benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)
add_subdirectory ("benchmark/gbench")

# Include sub-projects.
add_subdirectory ("ecs")
add_subdirectory ("unittest")
add_subdirectory ("benchmark")
add_subdirectory ("examples/example")
add_subdirectory ("examples/parallelism")
add_subdirectory ("examples/finite_state_machine")
add_subdirectory ("examples/mandelbrot")
add_subdirectory ("examples/shared_components")
add_subdirectory ("examples/tagged_components")
