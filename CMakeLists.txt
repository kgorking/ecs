cmake_minimum_required (VERSION 3.15)

project (ecs CXX)

# creates a library 'ecs' which is an interface (header files only)
add_library(ecs INTERFACE)

install(
    DIRECTORY include/ecs
    DESTINATION include
)

# determine whether this is a standalone project or included by other projects
set(ECS_STANDALONE_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(ECS_STANDALONE_PROJECT ON)
endif ()

if (MSVC)
    # warning level 4
    target_compile_options(ecs INTERFACE /std:c++latest /W4 /wo4648)
    #target_compile_options(ecs INTERFACE /fsanitize=address)

    # enable profiling
    #target_compile_options(ecs INTERFACE /Zi)
    #target_link_options(ecs INTERFACE /PROFILE)

    # Disable C++ exceptions.
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
    add_definitions(-D_HAS_EXCEPTIONS=0)

    # Disable RTTI.
    string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else()
    # lots of warnings
    target_compile_options(ecs INTERFACE -std=c++2a -Wall -Wextra -Wpedantic -Wshadow ${STDLIB})
    target_link_options(ecs INTERFACE -pthread)

    # Disable C++ exceptions.
    # - fails to compile on gcc 10
    if (Clang)
        string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
    endif()

    # Disable RTTI.
    # - clang's std::function::target needs rtti -_-
    if (GNU)
        string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
    endif()
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
    add_subdirectory ("examples/entt_example")
    add_subdirectory ("examples/example")
    add_subdirectory ("examples/filtering")
    add_subdirectory ("examples/frequency")
    add_subdirectory ("examples/finite_state_machine")
    add_subdirectory ("examples/global_component")
    add_subdirectory ("examples/hierarchy")
    add_subdirectory ("examples/mandelbrot")
    add_subdirectory ("examples/parallelism")
    add_subdirectory ("examples/plague_spell")
    add_subdirectory ("examples/pmr_allocator_support")
    add_subdirectory ("examples/pmr_custom_component")
    add_subdirectory ("examples/scheduler")
    add_subdirectory ("examples/sorting")
    add_subdirectory ("examples/tagged_components")

    # Unittests
    enable_testing()
    add_subdirectory ("unittest")
endif ()
