cmake_minimum_required (VERSION 3.8)

add_executable (ecs_bench "main.cpp" "ecs.cpp")
target_link_libraries(ecs_bench ecs benchmark)
