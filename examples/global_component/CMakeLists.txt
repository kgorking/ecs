cmake_minimum_required (VERSION 3.15)

add_executable (global_component "global_component.cpp")
target_link_libraries(global_component ecs)
