#ifndef ECS_JOBS_LAYOUT_H
#define ECS_JOBS_LAYOUT_H

#include "entity_range.h"

namespace ecs::detail {

// The location of a job
struct job_location {
	// The thread this job is scheduled on
	int thread_index;

	// The jobs position in the threads job vector
	int job_position;
};

// Details about a job
struct job_detail {
	// The entities this job spans
	entity_range entities;

	// The location of the job
	job_location loc;
};

} // namespace ecs::detail

#endif // !ECS_JOBS_LAYOUT_H
