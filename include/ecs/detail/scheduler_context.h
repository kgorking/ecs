#ifndef SCHEDULER_CONTEXT_H
#define SCHEDULER_CONTEXT_H

#include <unordered_map>
#include <vector>
#include <utility>

#include "type_hash.h"
#include "entity_range.h"
#include "job_detail.h"
#include "scheduler.h"

namespace ecs::detail {

class scheduler_context {
	// Cache of which threads has accessed which types and their ranges
	std::unordered_map<type_hash, std::vector<std::pair<entity_range, int>>> type_thread_map;

	// Remember what thread each range is scheduled on
	std::unordered_map<entity_range, job_location> range_thread_map;

	// The scheduler used
	scheduler& scheduler;

	std::vector<job_detail> job_details;

public:
	template<class Arg>
	int find_type_thread_index(Arg single_arg) {
		if constexpr (std::is_pointer_v<Arg>) {
			constexpr type_hash hash = get_type_hash<Arg>();
			auto const it = type_thread_map.find(hash);
			if (it != type_thread_map.end()) {
				for (auto const& pair : it->second) {
					if (pair.first.overlaps(range))
						return pair.second;
				}
			}
		}

		return -1;
	}

	// Find a possible thread index for a type and range
	template<class TupleArgs>
	int find_thread_index(entity_range range, TupleArgs const& args) {
		if (type_thread_map.size() == 0)
			return -1;

		return std::apply(
			[&](auto... split_args) {
				int const indices[] = {find_type_thread_index(split_args)...};
				for (int const index : indices) {
					if (index != -1)
						return index;
				}

				return -1;
			},
			args);
	}

	template <class TupleArgs>
	void insert_type_thread_index(entity_range range, TupleArgs const& args, int thread_index) {
		auto const insert_type_thread_index = [&](auto single_arg) {
			using arg_type = decltype(single_arg);

			if constexpr (std::is_pointer_v<arg_type>) {
				constexpr type_hash hash = get_type_hash<arg_type>();
				auto it = type_thread_map.find(hash);
				if (it != type_thread_map.end()) {
					for (auto const& pair : it->second) {
						if (pair.first.overlaps(range)) {
							// Type is already mapped, so do nothing
							return;
						}
					}
				} else {
					// Map new type to thread
					auto const result = type_thread_map.emplace(hash, std::vector<std::pair<entity_range, int>>{});
					Expects(result.second);
					it = result.first;
				}

				it->second.emplace_back(range, thread_index);
			}
		};

		std::apply(
			[&](auto... split_args) {
				(insert_type_thread_index(split_args), ...);
			},
			args);
	}

	void add_job_detail(entity_range rng, job_location loc) {
		job_details.emplace_back(rng, loc);
	}
};

}

#endif // !SCHEDULER_CONTEXT_H
