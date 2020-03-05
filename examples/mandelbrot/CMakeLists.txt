cmake_minimum_required (VERSION 3.15)

add_executable (mandelbrot "mandelbrot.cpp")
target_link_libraries(mandelbrot ecs)
