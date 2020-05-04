cmake_minimum_required (VERSION 3.15)

add_executable (scheduler "scheduler.cpp")
target_link_libraries(scheduler ecs)
