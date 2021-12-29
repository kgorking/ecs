#ifndef UNITTEST_H
#define UNITTEST_H

#include <ecs/ecs.h>

#if __cpp_lib_constexpr_vector && __cpp_constexpr_dynamic_alloc
#define CONSTEXPR_UNITTEST(t) static_assert((t))
#else
#define CONSTEXPR_UNITTEST(t) ((void)0)
#endif

#endif // !UNITTEST_H
