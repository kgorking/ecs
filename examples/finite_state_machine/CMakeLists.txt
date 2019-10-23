cmake_minimum_required (VERSION 3.8)

add_executable (finite_state_machine "finite_state_machine.cpp")
target_link_libraries(finite_state_machine ecs)
