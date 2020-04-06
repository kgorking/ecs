# CMakeList.txt : CMake project for ecs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)

add_library(ecs INTERFACE
	"component_pool.h"
	"component_pool_base.h"
	"component_specifier.h"
	"context.h"
	"entity.h"
	"entity_id.h"
	"entity_range.h"
	"runtime.h"
	"system.h"
	"system_base.h"
	"system_verification.h")

target_include_directories(ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
