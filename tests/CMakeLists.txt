
cmake_minimum_required(VERSION 3.8)

project(ProjectJsonLibTests)

# granular
add_executable(test-0 test-0.cxx)
target_link_libraries(test-0 json-lib)
add_test(NAME test-0 COMMAND test-0)

# large parse
set(json_file "${CMAKE_CURRENT_LIST_DIR}/data.json")
configure_file(test-1.cxx.in "${CMAKE_CURRENT_BINARY_DIR}/test-1.cxx" @ONLY)
add_executable(test-1 "${CMAKE_CURRENT_BINARY_DIR}/test-1.cxx")
target_link_libraries(test-1 json-lib)
add_test(NAME test-1 COMMAND test-1)
