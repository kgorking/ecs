cmake_minimum_required (VERSION 3.15)

add_executable (test_component_removal "component_removal.cpp")
target_link_libraries(test_component_removal ecs)
add_test(test_component_removal test_component_removal)

add_executable (test_component_pool "component_pool.cpp")
target_link_libraries(test_component_pool ecs)
add_test(test_component_pool test_component_pool)

add_executable (test_entity_range "entity_range.cpp")
target_link_libraries(test_entity_range ecs)
add_test(test_entity_range test_entity_range)

add_executable (test_runtime "runtime.cpp")
target_link_libraries(test_runtime ecs)
add_test(test_runtime test_runtime)

add_executable (test_shared_component "shared_component.cpp")
target_link_libraries(test_shared_component ecs)
add_test(test_shared_component test_shared_component)

add_executable (test_systems "system.cpp")
target_link_libraries(test_systems ecs)
add_test(test_systems test_systems)

add_executable (test_transient_components "transient_components.cpp")
target_link_libraries(test_transient_components ecs)
add_test(test_transient_components test_transient_components)

add_executable (test_global_component "global_component.cpp")
target_link_libraries(test_global_component ecs)
add_test(test_global_component test_global_component)

add_executable (test_sorting "sorting.cpp")
target_link_libraries(test_sorting ecs)
add_test(test_sorting test_sorting)

add_executable (test_filtering "filtering.cpp")
target_link_libraries(test_filtering ecs)
add_test(test_filtering test_filtering)

add_executable (test_scheduler "scheduler.cpp")
target_link_libraries(test_scheduler ecs)
add_test(test_scheduler test_scheduler)

add_executable (test_system_options "system_options.cpp")
target_link_libraries(test_system_options ecs)
add_test(test_system_options test_system_options)

add_executable (test_frequency "frequency.cpp")
target_link_libraries(test_frequency ecs)
add_test(test_frequency test_frequency)

add_executable (test_hierarchy "hierarchy.cpp")
target_link_libraries(test_hierarchy ecs)
add_test(test_hierarchy test_hierarchy)
