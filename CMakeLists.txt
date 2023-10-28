cmake_minimum_required(VERSION 3.22)
project(Tracer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)

if (BUILD_TESTS)
    add_executable(minitrace_test minitrace_test.cpp)
    add_executable(minitrace_test_mt minitrace_test_mt.cpp)
endif()

