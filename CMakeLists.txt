cmake_minimum_required(VERSION 3.16)
project(BPMAnalyzer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

option(BPMCORE_BUILD_TESTS "Build bpmcore unit tests" ON)

if(BPMCORE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)
