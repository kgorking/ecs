cmake_minimum_required (VERSION 3.15)

add_executable (unittests
	"unittest.cpp"
	
	"runtime.cpp"
	"component removal.cpp"
	"component_pool.cpp"
	"entity_range.cpp"
	"shared component.cpp"
	"system.cpp"
	"transient components.cpp"
	"global_component.cpp"
	"sorting.cpp"
	"filtering.cpp"
	"scheduler.cpp" "system_options.cpp" "frequency.cpp")
target_link_libraries(unittests ecs)

add_test(unittests unittests)
