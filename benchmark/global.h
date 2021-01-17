#pragma once


size_t constexpr start_components = 256;
// size_t constexpr num_components = 256 * 1024;
size_t constexpr num_components = start_components * start_components;

struct global_s {
	ecs_flags(ecs::flag::global);
	size_t dimension;
};

#define ECS_BENCHMARK_ONE(x) BENCHMARK(x)->Unit(benchmark::kMicrosecond)->Arg(1)
#define ECS_BENCHMARK(x) ECS_BENCHMARK_ONE(x)->Range(start_components, num_components)
