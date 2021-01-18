#pragma once

#include <ecs/flags.h>
#include <ecs/entity_id.h>

size_t constexpr start_components = 1024;
// size_t constexpr num_components = 256 * 1024;
size_t constexpr num_components = start_components * start_components;
//size_t constexpr num_components = 256 * 256;

struct global_s {
	ecs_flags(ecs::flag::global);
	size_t dimension;
};

#define ECS_BENCHMARK_ONE(x) BENCHMARK(x)/*->Unit(benchmark::kMicrosecond)*/->Arg(1)
#define ECS_BENCHMARK(x) BENCHMARK(x)->Range(start_components, num_components)

extern void benchmark_system(ecs::entity_id ent, int &color, global_s const &global);
