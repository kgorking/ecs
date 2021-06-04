cmake_minimum_required (VERSION 3.15)

add_executable (scheduler_layout_demo "scheduler_layout_demo.cpp")
target_compile_options(scheduler_layout_demo PUBLIC /bigobj)
target_link_libraries(scheduler_layout_demo ecs)
