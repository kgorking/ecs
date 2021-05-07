cmake_minimum_required (VERSION 3.15)

add_executable (pmr_allocator_support "pmr_allocator_support.cpp")
target_link_libraries(pmr_allocator_support ecs)
