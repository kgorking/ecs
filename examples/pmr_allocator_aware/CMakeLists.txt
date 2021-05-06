cmake_minimum_required (VERSION 3.15)

add_executable (pmr_allocator_aware "pmr_allocator_aware.cpp")
target_link_libraries(pmr_allocator_aware ecs)
