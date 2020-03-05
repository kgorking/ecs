cmake_minimum_required (VERSION 3.15)

add_executable (parallelism "parallelism.cpp")
target_link_libraries(parallelism ecs)
