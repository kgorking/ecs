cmake_minimum_required (VERSION 3.15)

# Use google benchmark
include(FetchContent)
FetchContent_Declare(gbench
  GIT_REPOSITORY "https://github.com/google/benchmark.git"
  GIT_TAG "v1.8.3"
  SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/gbench"
)
FetchContent_MakeAvailable(gbench)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)

add_executable (ecs_bench "main.cpp" "bench_component.cpp" "bench_baseline.cpp" "bench_ranged.cpp" "bench_sorting.cpp" "bench_hierarchy.cpp" "bench_particles.cpp" "shared_system.cpp")
target_link_libraries(ecs_bench ecs benchmark::benchmark)

add_executable (ecs_bench_lower_bound "bench_lower_bound.cpp")
target_link_libraries(ecs_bench_lower_bound benchmark::benchmark)
