﻿cmake_minimum_required (VERSION 3.15)

add_executable (custom_contract_violation_handler "custom_contract_violation_handler.cpp")
target_link_libraries(custom_contract_violation_handler ecs)
