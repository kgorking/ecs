cmake_minimum_required (VERSION 3.15)

add_executable (unittests
	"unittest.cpp"
	"constexpr.cpp"
	"runtime.cpp"
	"component removal.cpp"
	"component_pool.cpp"
	
	
	"entity_range.cpp"
	
	"shared component.cpp"
	
	"system.cpp"
	"transient components.cpp"
	
	 "global_component.cpp")
target_link_libraries(unittests ecs)


add_test(unittests unittests)
