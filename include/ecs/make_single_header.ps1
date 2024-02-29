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
	'detail/tagged_pointer.h',
	'entity_id.h',
	'detail/entity_iterator.h',
	'detail/options.h',
	'entity_range.h',
	'detail/parent_id.h',
	'detail/variant.h',
	'flags.h',
	'detail/stride_view.h',
	'detail/component_pool_base.h',
	'detail/component_pool.h',
	'detail/system_defs.h',
	'detail/component_pools.h',
	'options.h',
	'parent.h',
	'detail/interval_limiter.h',
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
$sys_headers = '// Auto-generated single-header include file
#if 0 //defined(__cpp_lib_modules)
#if defined(_MSC_VER) && _MSC_VER <= 1939
import std.core;
#else
import std;
#endif
#else
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <execution>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#endif
'

# Write out module
"module;
$sys_headers
export module ecs;
#define ECS_EXPORT export
" > ecs.ixx

# Write out single-include header
"$sys_headers
#ifndef ECS_EXPORT
#define ECS_EXPORT
#endif
" > ecs_sh.h

# Filter out the local includes from the content of each header and pipe it to ecs_sh.h
$filtered = (sls -Path $files -SimpleMatch -Pattern '#include' -NotMatch).Line

$filtered >> ecs.ixx
$filtered >> ecs_sh.h
