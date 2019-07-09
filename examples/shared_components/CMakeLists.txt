cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories(../../)
include_directories(../../GSL/include)
add_executable (shared_components "shared_components.cpp")
