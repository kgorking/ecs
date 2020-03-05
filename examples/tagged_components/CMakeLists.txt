cmake_minimum_required (VERSION 3.15)

add_executable (tagged_components "tagged_components.cpp")
target_link_libraries(tagged_components ecs)
