#pragma once


//size_t constexpr num_components = 1;
//size_t constexpr num_components = 32;
//size_t constexpr num_components = 4'000;
//size_t constexpr num_components = 32'000;
size_t constexpr num_components = 256 * 1'024;
//size_t constexpr num_components = 1'000'000;
//size_t constexpr num_components = 16'000'000;
//size_t constexpr num_components = 1'000'000'000;

struct global_s {
	ecs_flags(ecs::flag::global);
	size_t dimension;
};

