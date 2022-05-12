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
    target_compile_options(ecs INTERFACE
        /std:c++latest

        # enable lots of warnings
        /W4

        # -- Dump compilation info ---
        #/Bt                     # total time spent in frontend (d1) and backend (d2)
        #/d1templateStats        # show info regarding template use
        #/d1reportTimeSummary    # show frontend compile time summary
        #/d1reportTime           # show (very) detailed frontend compile time info
        #/d2cgsummary            # show backend compile time summary

        # -- Enable conversion warnings --
        /w44191     # 'operator': unsafe conversion from 'type_of_expression' to 'type_required'.
        /w44242     # 'identifier': conversion from 'type1' to 'type2', possible loss of data.
        /w44254     # 'operator': conversion from 'type1' to 'type2', possible loss of data.
        /w44800     # Implicit conversion from 'type' to bool. Possible information loss.
        /w44826     # Conversion from 'type1' to 'type2' is sign-extended. This may cause unexpected runtime behavior.
        /w45219     # implicit conversion from 'type-1' to 'type-2', possible loss of data.
        #/w44388     # signed/unsigned mismatch
        #/w44365     # 'action': conversion from 'type_1' to 'type_2', signed/unsigned mismatch.

		# Shows the layout of the struct name 'chunk'
		#/d1reportSingleClassLayoutchunk

        # Enable address sanitizer
        #/fsanitize=address
    )

    # enable profiling
    #target_compile_options(ecs INTERFACE /Zi)
    #target_link_options(ecs INTERFACE /PROFILE)

    # Disable C++ exceptions.
    #string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
    #add_definitions(-D_HAS_EXCEPTIONS=0)

    # Disable RTTI.
    #string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else()
    # lots of warnings
    target_compile_options(ecs INTERFACE
        -std=c++2a
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        #-Wsign-conversion
        #-ftime-trace  # produce json flamegraph files. open with chrome://tracing or edge://tracing
	)
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
    add_subdirectory ("examples/interval")
    add_subdirectory ("examples/finite_state_machine")
    add_subdirectory ("examples/global_component")
    add_subdirectory ("examples/hierarchy")
    add_subdirectory ("examples/mandelbrot")
    add_subdirectory ("examples/parallelism")
    add_subdirectory ("examples/plague_spell")
    add_subdirectory ("examples/scheduler")
    add_subdirectory ("examples/sorting")
    add_subdirectory ("examples/tagged_components")

    # Unittests
    enable_testing()
    add_subdirectory ("unittest")
endif ()
