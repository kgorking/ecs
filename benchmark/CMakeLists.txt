cmake_minimum_required (VERSION 3.15)

add_executable (ecs_bench "main.cpp" "system.cpp" "component.cpp")
target_link_libraries(ecs_bench ecs benchmark)
