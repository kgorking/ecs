# CMakeList.txt : CMake project for ecs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)

add_library(ecs INTERFACE)
target_link_libraries(ecs INTERFACE GSL)
target_include_directories(ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
