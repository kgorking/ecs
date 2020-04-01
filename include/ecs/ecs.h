#include <algorithm>
#include <concepts>
#include <execution>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <span>
#include <type_traits>
#include <tuple>
#include <typeinfo>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

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
