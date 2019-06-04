cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories(../../)
include_directories(../../GSL/include)
add_executable (shared_components "shared_components.cpp")

# C++ 17 is required
set_property(TARGET shared_components PROPERTY CXX_STANDARD 17)
set_property(TARGET shared_components PROPERTY CXX_STANDARD_REQUIRED on)
