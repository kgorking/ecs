cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories(../../)
include_directories(../../GSL/include)
add_executable (finite_state_machine "finite_state_machine.cpp")

# C++ 17 is required
set_property(TARGET finite_state_machine PROPERTY CXX_STANDARD 17)
set_property(TARGET concurrency PROPERTY CXX_STANDARD_REQUIRED on)
