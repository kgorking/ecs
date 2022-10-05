# Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# Set output to utf-8
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'

# All the files to concat into the single header
$files = (
	'../../tls/include/tls/cache.h',
	'../../tls/include/tls/split.h',
	'../../tls/include/tls/collect.h',
	'detail/type_list.h',
	'detail/contract.h',
	'detail/type_hash.h',
	'entity_id.h',
	'detail/entity_iterator.h',
	'entity_range.h',
	'parent.h',
	'detail/parent_id.h',
	'flags.h',
	'detail/flags.h',
	'options.h',
	'detail/options.h',
	'detail/component_pool_base.h',
	'detail/component_pool.h',
	'detail/interval_limiter.h',
	'detail/system_defs.h',
	'detail/pool_entity_walker.h',
	'detail/pool_range_walker.h',
	'detail/entity_offset.h',
	'detail/verification.h',
	'detail/entity_range.h',
	'detail/find_entity_pool_intersections.h',
	'detail/system_base.h',
	'detail/system.h',
	'detail/system_sorted.h',
	'detail/system_ranged.h',
	'detail/system_hierachy.h',
	'detail/system_global.h',
	'detail/scheduler.h',
	'detail/context.h',
	'runtime.h')

# Write all system includes
'// Auto-generated single-header include file
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <execution>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <mutex> // needed for scoped_lock
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

' > ecs_sh.h

# Filter out the local includes from the content of each header and pipe it to ecs_sh.h
(sls -Path $files -SimpleMatch -Pattern '#include' -NotMatch).Line >> ecs_sh.h
