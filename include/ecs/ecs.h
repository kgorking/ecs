#if defined(ECS_USE_MODULES)
import ecs;
#else
#if 0
#include "runtime.h"
#else
// test single-include header
#include "ecs_sh.h"
#endif
#endif
