cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories(../)
include_directories(../GSL/include)

add_executable (ecs_bench "main.cpp" "ecs.cpp" "helper.cpp")

target_link_libraries(ecs_bench benchmark)
