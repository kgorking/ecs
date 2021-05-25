#ifndef ECS_DETAIL_JOB_H
#define ECS_DETAIL_JOB_H

#include <memory>

namespace ecs::detail {

class scheduler_job {
	struct job_base {
		virtual void call() = 0;
		virtual ~job_base(){};
	};

	template <class F>
	struct job_impl : job_base {
		F f;

		job_impl(F&& f_) : f(std::forward<F>(f_)) {}

		void call() {
			f();
		}
	};

	std::unique_ptr<job_base> job;

public:
	template <class F>
	scheduler_job(F&& f) : job(std::make_unique<job_impl<F>>(std::forward<F>(f))) {}

	scheduler_job() = default;
	scheduler_job(scheduler_job &&) = default;
	scheduler_job& operator=(scheduler_job &&) = default;
	scheduler_job(scheduler_job const&) = delete;
	scheduler_job(scheduler_job &) = delete;
	scheduler_job& operator=(scheduler_job const&) = delete;

	void operator ()() {
		Expects(nullptr != job.get());
		job->call();
	}
};

}

#endif // !ECS_DETAIL_JOB_H
