#include <ecs/ecs.h>
#include "gbench/include/benchmark/benchmark.h"
#include <random>
#include <numeric>
#include <ranges>

#include "global.h"

using test_component_type = size_t;
auto constexpr test_component = test_component_type{9};

void component_add_spans(benchmark::State& state) {
	auto const range = static_cast<std::size_t>(state.range(0));
	auto const nentities = static_cast<int>(state.range(0));

	std::vector<test_component_type> ints(range + 1);
	std::iota(ints.begin(), ints.end(), 9);

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component_span({0, nentities}, ints);
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_spans);

void component_add_generator(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component_generator({0, nentities}, [i = 0](ecs::entity_id ) mutable {
			return i++;
		});
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_generator);

void component_add(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities}, test_component);
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add);

void component_add_1k_blocks(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		for (ecs::entity_id i = 0; i < nentities; i += 1024) {
			ecs.add_component({i, i + 1023}, test_component);
			ecs.commit_changes();
		}
	}
}
ECS_BENCHMARK(component_add_1k_blocks);

void component_add_half_front(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({nentities / 2 + 1, nentities}, test_component);
		ecs.commit_changes();
		ecs.add_component({0, nentities / 2}, test_component);
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_half_front);

void component_add_half_back(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities / 2}, test_component);
		ecs.commit_changes();
		ecs.add_component({nentities / 2 + 1, nentities}, test_component);
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_half_back);

// This is currently the worst case scenario. Every commit will move all other components as well.
void component_insert_worst_case(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));
	constexpr int block_size = 256;

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;

		for (auto i = nentities; i >= 0; i -= block_size) {
			ecs.add_component({i - block_size + 1, i}, test_component);
			ecs.commit_changes();
		}
	}
}
ECS_BENCHMARK(component_insert_worst_case);

void component_add_remove_all(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities}, test_component);
		ecs.commit_changes();
		ecs.remove_component<test_component_type>({0, nentities});
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_remove_all);

void component_add_remove_half_front(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities}, test_component);
		ecs.commit_changes();
		ecs.remove_component<test_component_type>({0, nentities / 2});
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_remove_half_front);

void component_add_remove_half_back(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities}, test_component);
		ecs.commit_changes();
		ecs.remove_component<test_component_type>({nentities / 2 + 1, nentities});
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_remove_half_back);

void component_add_remove_half_middle(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities}, test_component);
		ecs.commit_changes();
		ecs.remove_component<test_component_type>({nentities / 4, nentities - nentities / 4});
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_remove_half_middle);

void component_randomized_add(benchmark::State& state) {
	auto const range = static_cast<std::size_t>(state.range(0));

	std::vector<int> ids(range);
	std::iota(ids.begin(), ids.end(), 0);
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(ids.begin(), ids.end(), g);

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		for (auto id : ids) {
			ecs.add_component(id, test_component);
		}
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_randomized_add);

void component_add_randomized_remove(benchmark::State& state) {
	auto const range = static_cast<std::size_t>(state.range(0));
	auto const nentities = static_cast<int>(state.range(0));

	std::vector<int> ids(range);
	std::iota(ids.begin(), ids.end(), 0);
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(ids.begin(), ids.end(), g);

	for ([[maybe_unused]] auto const _ : state) {
		ecs::runtime ecs;
		ecs.add_component({0, nentities - 1}, test_component);
		ecs.commit_changes();

		for (auto id : ids) {
			ecs.remove_component<test_component_type>(id);
		}
		ecs.commit_changes();
	}
}
ECS_BENCHMARK(component_add_randomized_remove);

void find_component_data(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::detail::component_pool<int> pool;

	for (ecs::entity_id i = 0; i < nentities; i += 8) {
		pool.add({i, i + 7}, int{});
		pool.process_changes();
	}

	for ([[maybe_unused]] auto const _ : state) {
		for (ecs::entity_id i = 0; i < nentities; ++i) {
			auto* val = pool.find_component_data(i);
			benchmark::DoNotOptimize(val);
		}
	}
}
ECS_BENCHMARK(find_component_data);


void find_component_data_random(benchmark::State& state) {
	auto const nentities = static_cast<int>(state.range(0));

	ecs::detail::component_pool<int> pool;

	for (ecs::entity_id i = 0; i < nentities; i += 8) {
		pool.add({i, i + 7}, int{});
		pool.process_changes();
	}

	std::random_device rd;
	std::mt19937 g(rd());
	std::vector<int> indices(nentities, 0);
	std::iota(indices.begin(), indices.end(), 0);
	std::ranges::shuffle(indices, g);

	for ([[maybe_unused]] auto const _ : state) {
		for (ecs::entity_id i = 0; i < nentities; ++i) {
			auto const id = indices[i];
			auto* val = pool.find_component_data(id);
			benchmark::DoNotOptimize(val);
		}
	}
}
ECS_BENCHMARK(find_component_data_random);
