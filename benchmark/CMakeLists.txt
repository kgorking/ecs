cmake_minimum_required (VERSION 3.15)

add_executable (ecs_bench "main.cpp" "shared_system.cpp" "bench_component.cpp" "bench_baseline.cpp" "bench_ranged.cpp" "bench_sorting.cpp" "bench_hierarchy.cpp" "bench_particles.cpp")
target_link_libraries(ecs_bench ecs benchmark)

add_executable (ecs_bench_lower_bound "bench_lower_bound.cpp")
target_link_libraries(ecs_bench_lower_bound benchmark)
#target_compile_options(ecs_bench_lower_bound PUBLIC /arch:AVX512)
