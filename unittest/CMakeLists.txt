cmake_minimum_required (VERSION 3.8)

add_executable (unittest_component_deletion "unittest.cpp" "component deletion.cpp")
target_link_libraries(unittest_component_deletion ecs)

add_executable (unittest_component_pool "unittest.cpp" "component_pool.cpp")
target_link_libraries(unittest_component_pool ecs)

add_executable (unittest_entity_sorting "unittest.cpp" "entity sorting.cpp")
target_link_libraries(unittest_entity_sorting ecs)

add_executable (unittest_missing_storage "unittest.cpp" "missing storage.cpp")
target_link_libraries(unittest_missing_storage ecs)

add_executable (unittest_mutable_lambdas "unittest.cpp" "mutable lambdas.cpp")
target_link_libraries(unittest_mutable_lambdas ecs)

add_executable (unittest_range_intersect "unittest.cpp" "range intersect.cpp")
target_link_libraries(unittest_range_intersect ecs)

add_executable (unittest_ranged_add "unittest.cpp" "ranged add.cpp")
target_link_libraries(unittest_ranged_add ecs)

add_executable (unittest_shared_component "unittest.cpp" "shared component.cpp")
target_link_libraries(unittest_shared_component ecs)

add_executable (unittest_system_argument_ordering "unittest.cpp" "system argument ordering.cpp")
target_link_libraries(unittest_system_argument_ordering ecs)

add_executable (unittest_system "unittest.cpp" "system.cpp")
target_link_libraries(unittest_system ecs)

add_executable (unittest_transient_components "unittest.cpp" "transient components.cpp")
target_link_libraries(unittest_transient_components ecs)

add_executable (unittest_two_systems_and_components "unittest.cpp" "two systems and components.cpp")
target_link_libraries(unittest_two_systems_and_components ecs)

add_executable (unittest_two_component_system "unittest.cpp" "two-component system.cpp")
target_link_libraries(unittest_two_component_system ecs)


add_test(component_deletion unittest_component_deletion)
add_test(component_pool unittest_component_pool)
add_test(entity_sorting unittest_entity_sorting)
add_test(missing_storage unittest_missing_storage)
add_test(mutable_lambdas unittest_mutable_lambdas)
add_test(range_intersect unittest_range_intersect)
add_test(ranged_add unittest_ranged_add)
add_test(shared_component unittest_shared_component)
add_test(system_argument_ordering unittest_system_argument_ordering)
add_test(system unittest_system)
add_test(transient_components unittest_transient_components)
add_test(two_systems_and_components unittest_two_systems_and_components)
add_test(two_component_system unittest_two_component_system)
