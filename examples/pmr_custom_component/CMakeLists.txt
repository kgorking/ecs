cmake_minimum_required (VERSION 3.15)

add_executable (pmr_custom_component "pmr_custom_component.cpp")
target_link_libraries(pmr_custom_component ecs)
