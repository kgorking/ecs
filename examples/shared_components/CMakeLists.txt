cmake_minimum_required (VERSION 3.15)

add_executable (shared_components "shared_components.cpp")
target_link_libraries(shared_components ecs)
