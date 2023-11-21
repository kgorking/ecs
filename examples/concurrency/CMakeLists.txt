cmake_minimum_required (VERSION 3.15)

add_executable (concurrency "concurrency.cpp")
target_link_libraries(concurrency ecs)
