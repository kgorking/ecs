cmake_minimum_required (VERSION 3.8)

add_executable (unittests
	"unittest.cpp"
	"runtime.cpp"
	"component deletion.cpp"
	"component_pool.cpp"
	"entity sorting.cpp"
	"missing storage.cpp"
	"mutable lambdas.cpp"
	"range intersect.cpp"
	"ranged add.cpp"
	"shared component.cpp"
	"system argument ordering.cpp"
	"system.cpp"
	"transient components.cpp"
	"two systems and components.cpp"
	"two-component system.cpp")
target_link_libraries(unittests ecs)


add_test(unittests unittests)
