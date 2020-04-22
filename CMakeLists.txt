cmake_minimum_required (VERSION 3.15)

project (ecs CXX)

# creates a library 'ecs' which is an interface (header files only)
add_library(ecs INTERFACE)

install(
    DIRECTORY include/ecs
    DESTINATION include
)

# Require c++20 and concepts
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED on)

# determine whether this is a standalone project or included by other projects
set(ECS_STANDALONE_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(ECS_STANDALONE_PROJECT ON)
endif ()

if (MSVC)
    # warning level 4
    target_compile_options(ecs INTERFACE /W4)
else()
    # lots of warnings
    target_compile_options(ecs INTERFACE -Wall -Wextra -pedantic)
endif()

# Project headers
# add include folders to the library and targets that consume it
# the SYSTEM keyword suppresses warnings for users of the library
if(ECS_STANDALONE_PROJECT)
    target_include_directories(ecs INTERFACE
        $<BUILD_INTERFACE:
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        >
    )
else()
    target_include_directories(ecs SYSTEM INTERFACE
        $<BUILD_INTERFACE:
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        >
    )
endif()

# add tls library
add_subdirectory ("tls")
target_link_libraries(ecs INTERFACE tls)

if (ECS_STANDALONE_PROJECT)
    # Benchmark
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)
    add_subdirectory ("benchmark/gbench")
    add_subdirectory ("benchmark")

    # Examples
    add_subdirectory ("examples/example")
    add_subdirectory ("examples/plague_spell")
    add_subdirectory ("examples/entt_example")
    add_subdirectory ("examples/parallelism")
    add_subdirectory ("examples/finite_state_machine")
    add_subdirectory ("examples/mandelbrot")
    add_subdirectory ("examples/shared_components")
    add_subdirectory ("examples/tagged_components")

    # Unittests
    enable_testing()
    add_subdirectory ("unittest")
endif ()
