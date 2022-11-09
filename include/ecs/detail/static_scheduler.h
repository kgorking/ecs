#ifndef ECS_DETAIL_STATIC_SCHEDULER_H
#define ECS_DETAIL_STATIC_SCHEDULER_H

#include <thread>
#include <vector>
#include "entity_range.h"

//
// Stuff to do before work on the scheduler can proceed:
//   * unify the argument creation across systems
//   * disallow mutable user supplied system lambdas
//
namespace ecs::detail {

    struct operation {
        template<typename Arguments, typename Fn>
        explicit operation(Arguments &args, Fn& fn)
            : arguments{&args}
            , function(&fn)
            , op{[](entity_range range, void *p1, void *p2){
                auto *args = static_cast<Arguments*>(p1);
                auto *func = static_cast<Fn*>(p2);
                (*args)(range, *func);
            }}
        {}

        void run(entity_range range) {
            op(range, arguments, function);
        }

    private:
        void* arguments;
        void* function;
        void (*op)(entity_range range, void*, void*);
    };

    class job {
        //entity_range range;
        operation op;
    };

    class thread_lane {
        // The vector of jobs to run on this thread
        std::vector<job> jobs;

        // The stl thread object
        std::thread thread;

        // Time it took to run all jobs
        double time = 0.0;
    };

    class static_scheduler {};

}

#endif // !ECS_DETAIL_STATIC_SCHEDULER_H
