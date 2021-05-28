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
	void (scheduler_job::*pre_job)(void) = &scheduler_job::init_pre_job;

	std::unique_ptr<std::barrier<>> incoming_barrier{std::make_unique<std::barrier<>>(1)};
	std::vector<std::barrier<>*> outgoing_barriers;

	std::bitset<256> threads_in;
	std::bitset<256> threads_out;


private:
	void do_pre_job() {
#ifndef ECS_SCHEDULER_LAYOUT_DEMO
		incoming_barrier->arrive_and_wait();
#endif
	}

	void init_pre_job() {
		static std::mutex m;
		std::scoped_lock sl(m);

		if (pre_job == &scheduler_job::init_pre_job) {
			auto const num_threads = threads_in.count();
			if (num_threads > 1) {
				// Other threads are arriving here, so set up a barrier
				//incoming_barrier = std::make_unique<std::barrier<>>(num_threads);
				update_barrier_counter();
				pre_job = &scheduler_job::do_pre_job;
			} else {
				// No synchronization needed, do nothing
				pre_job = &scheduler_job::do_nothing;
			}
		}

		// Call the job
		Ensures(pre_job != &scheduler_job::init_pre_job);
		(this->*pre_job)();
	}
	void do_nothing() {}

	void update_barrier_counter() {
		std::destroy_at(incoming_barrier.get());
		std::construct_at(incoming_barrier.get(), threads_in.count());
	}

public:
	scheduler_job() = default;
	scheduler_job(scheduler_job&&) = default;
	scheduler_job& operator=(scheduler_job&&) = default;
	scheduler_job(scheduler_job const&) = delete;
	scheduler_job(scheduler_job&) = delete;
	scheduler_job& operator=(scheduler_job const&) = delete;

	template <class F, class Args>
	scheduler_job(entity_range range, Args args, F&& f)
		: job(std::make_unique<job_impl<F, Args>>(range, args, std::forward<F>(f))) {}

	void operator()() {
		Expects(nullptr != job.get());

		(this->*pre_job)();
		job->call();

		for (auto b : outgoing_barriers) {
			[[maybe_unused]] auto const x = b->arrive();
		}
	}

	std::barrier<>* get_barrier() {
		return incoming_barrier.get();
	}

	void add_outgoing_barrier(std::barrier<>* outgoing_barrier) {
		outgoing_barriers.push_back(outgoing_barrier);
	}

	void set_incoming_thread(int thread_index) {
		Expects(thread_index >= 0 && thread_index < 256);
		threads_in.set(thread_index);
	}

	bool test_incoming_thread(int thread_index) {
		Expects(thread_index >= 0 && thread_index < 256);
		return threads_in.test(thread_index);
	}

	void set_outgoing_thread(int thread_index) {
		Expects(thread_index >= 0 && thread_index < 256);
		threads_out.set(thread_index);
	}

	bool test_outgoing_thread(int thread_index) {
		Expects(thread_index >= 0 && thread_index < 256);
		return threads_out.test(thread_index);
	}
};

} // namespace ecs::detail

#endif // !ECS_DETAIL_JOB_H
