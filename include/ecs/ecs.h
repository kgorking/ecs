#include <concepts>
#include <utility>
#include <algorithm>
#include <type_traits>
#include <optional>
#include <variant>
#include <utility>
#include <vector>
#include <functional>
#include <tuple>
#include <map>
#include <typeindex>
#include <shared_mutex>
#include <execution>
#include <span>

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond) ((cond) ? static_cast<void>(0) : std::terminate())
#define Ensures(cond) ((cond) ? static_cast<void>(0) : std::terminate())

#include "entity_id.h"
#include "entity.h"
#include "entity_range.h"

#include "../threaded/threaded/threaded.h"
#include "component_specifier.h"
#include "component_pool_base.h"
#include "component_pool.h"

#include "system_verification.h"
#include "system_base.h"
#include "system.h"

#include "context.h"
#include "runtime.h"
