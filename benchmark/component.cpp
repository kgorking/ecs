#include "gbench/include/benchmark/benchmark.h"
#include <ecs/ecs.h>
#include <random>

#include "shared.h"

void component_add(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, size_t{});
        ecs::commit_changes();
    }
}
BENCHMARK(component_add)->Arg(num_components);

void component_add_half_front(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({nentities / 2 + 1, nentities}, size_t{});
        ecs::commit_changes();

        ecs::add_component({0, nentities / 2}, size_t{});
        ecs::commit_changes();
    }
}
BENCHMARK(component_add_half_front)->Arg(num_components);

void component_add_half_back(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities / 2}, size_t{});
        ecs::commit_changes();

        ecs::add_component({nentities / 2 + 1, nentities}, size_t{});
        ecs::commit_changes();
    }
}
BENCHMARK(component_add_half_back)->Arg(num_components);

void component_remove_all(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
        ecs::commit_changes();

        ecs::remove_component<int>({0, nentities});
        ecs::commit_changes();
    }
}
BENCHMARK(component_remove_all)->Arg(num_components);

void component_remove_half_front(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
        ecs::commit_changes();

        ecs::remove_component<int>({0, nentities / 2});
        ecs::commit_changes();
    }
}
BENCHMARK(component_remove_half_front)->Arg(num_components);

void component_remove_half_back(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
        ecs::commit_changes();

        ecs::remove_component<int>({nentities / 2 + 1, nentities});
        ecs::commit_changes();
    }
}
BENCHMARK(component_remove_half_back)->Arg(num_components);

void component_remove_half_middle(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        ecs::add_component({0, nentities}, int{});
        ecs::commit_changes();

        ecs::remove_component<int>({nentities / 4, nentities - nentities / 4});
        ecs::commit_changes();
    }
}
BENCHMARK(component_remove_half_middle)->Arg(num_components);

void component_randomized_add(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    std::vector<ecs::entity_id> ids;
    ids.reserve(nentities);
    std::generate_n(std::back_inserter(ids), nentities, [i = ecs::entity_id{0}]() mutable { return i++; });
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(ids.begin(), ids.end(), g);

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        for (auto id : ids) {
            ecs::add_component(id, int{});
        }
        ecs::commit_changes();
    }
}
BENCHMARK(component_randomized_add)->Arg(num_components);

void component_randomized_remove(benchmark::State& state) {
    auto const nentities = static_cast<ecs::detail::entity_type>(state.range(0));

    std::vector<ecs::entity_id> ids;
    ids.reserve(nentities);
    std::generate_n(std::back_inserter(ids), nentities, [i = ecs::entity_id{0}]() mutable { return i++; });
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(ids.begin(), ids.end(), g);

    for ([[maybe_unused]] auto const _ : state) {
        ecs::detail::_context.reset();

        state.PauseTiming();
        ecs::add_component({0, nentities-1}, int{});
        ecs::commit_changes();
        state.ResumeTiming();

        for (auto id : ids) {
            ecs::remove_component<int>(id);
        }
        ecs::commit_changes();
    }
}
BENCHMARK(component_randomized_remove)->Arg(num_components);
