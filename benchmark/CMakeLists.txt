cmake_minimum_required (VERSION 3.15)

add_executable (ecs_bench "main.cpp" "bench_hierarchy.cpp")# "bench_ranged.cpp" "bench_baseline.cpp" "bench_particles.cpp" "bench_component.cpp")
target_link_libraries(ecs_bench ecs benchmark)
