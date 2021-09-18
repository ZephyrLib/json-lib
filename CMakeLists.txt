
cmake_minimum_required(VERSION 3.8)

project(ProjectJsonLib)

option(JSON_TESTS "JSON_TESTS" ON)

add_library(json-lib INTERFACE)
target_include_directories(json-lib INTERFACE include)

if(JSON_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
