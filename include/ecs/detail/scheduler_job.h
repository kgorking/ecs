#ifndef ECS_DETAIL_JOB_H
#define ECS_DETAIL_JOB_H

#include <barrier>
#include <bitset>
#include <memory>
#include <semaphore>

namespace ecs::detail {

class scheduler_job {
	struct job_base {
		virtual void call() = 0;
		virtual ~job_base(){};
	};

	template <class F, class Args>
	struct job_impl : job_base {
		F f;
		entity_range const range;
		Args args;

		job_impl(entity_range range_, Args args_, F&& f_) : range(range_), args(args_), f(std::forward<F>(f_)) {}

		void call() override {
			for (entity_id const ent : range) {
				f(ent, args);
			}
		}
	};

	template <class F>
	struct job_simple_impl : job_base {
		F f;

		job_simple_impl(F&& f_) : f(std::forward<F>(f_)) {}

		void call() override {
			f();
		}
	};

	std::unique_ptr<job_base> job;

	std::unique_ptr<std::barrier<>> incoming_barrier{nullptr};
	std::vector<std::barrier<>*> outgoing_barriers;

	int jobs_in{0};


private:
	void update_barrier_counter() {
		if (!incoming_barrier) {
			incoming_barrier = std::make_unique<std::barrier<>>(1 + jobs_in);
		} else {
			std::destroy_at(incoming_barrier.get());
			// arrive and wait on this thread, arrive on other threads
			std::construct_at(incoming_barrier.get(), 1 + jobs_in);
		}
	}

public:
	scheduler_job() = default;
	~scheduler_job() = default;
	scheduler_job(scheduler_job&&) = default;
	scheduler_job(scheduler_job const&) = delete;
	scheduler_job(scheduler_job&) = delete;
	scheduler_job& operator=(scheduler_job const&) = delete;
	scheduler_job& operator=(scheduler_job&&) = default;

	template <class F, class Args>
	scheduler_job(entity_range range, Args args, F&& f)
		: job(std::make_unique<job_impl<F, Args>>(range, args, std::forward<F>(f))) {}

	void operator()() {
		Expects(nullptr != job.get());

		if (incoming_barrier) {
			incoming_barrier->arrive_and_wait();
		}

		job->call();

		for (std::barrier<>* barrier : outgoing_barriers) {
			// Notify other threads that our work here is done
			(void) barrier->arrive();
		}
	}

	std::barrier<>* get_barrier() {
		return incoming_barrier.get();
	}

	void add_outgoing_barrier(std::barrier<>* outgoing_barrier) {
		Expects(incoming_barrier.get() != outgoing_barrier);
		Expects(std::ranges::find(outgoing_barriers, outgoing_barrier) == outgoing_barriers.end());
		outgoing_barriers.push_back(outgoing_barrier);
	}

	void increase_incoming_job_count() {
		jobs_in += 1;
		update_barrier_counter();
	}
};

} // namespace ecs::detail

#endif // !ECS_DETAIL_JOB_H
