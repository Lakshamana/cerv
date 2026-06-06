cmake_minimum_required(VERSION 3.20)
project(cerv C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cerv_core STATIC
  src/server.c
  src/router.c
  src/request.c
  src/response.c
  src/task.c
  src/arena.c
)
target_include_directories(cerv_core PUBLIC include)
target_compile_options(cerv_core PRIVATE -Wall -Wextra -Wpedantic)

find_package(llhttp CONFIG REQUIRED)
target_link_libraries(cerv_core PUBLIC llhttp::llhttp_static)

enable_testing()

add_executable(test_router tests/unit/test_router.c src/router.c)
add_executable(test_request tests/unit/test_request.c src/request.c src/arena.c)
target_link_libraries(test_request PRIVATE llhttp::llhttp_static)
add_executable(hello examples/hello.c)
target_link_libraries(hello PRIVATE cerv_core)
target_include_directories(test_router PRIVATE include tests)
target_include_directories(test_request PRIVATE include tests)
add_executable(test_arena tests/unit/test_arena.c src/arena.c)
target_include_directories(test_arena PRIVATE include tests)
add_executable(test_response tests/unit/test_response.c src/response.c src/arena.c)
target_include_directories(test_response PRIVATE include tests)
add_executable(test_chan tests/unit/test_chan.c src/chan.c)
target_include_directories(test_chan PRIVATE include tests)
# Scan source file for `static void test_*` functions and register each as its
# own CTest entry — no manual listing needed when adding new tests.
function(cerv_discover_tests target source)
  file(STRINGS ${source} _lines REGEX "^static void (test_[a-zA-Z0-9_]+)\\(")
  foreach(_line ${_lines})
    string(REGEX MATCH "test_[a-zA-Z0-9_]+" _name ${_line})
    add_test(NAME ${target}::${_name} COMMAND ${target} ${_name})
  endforeach()
endfunction()

cerv_discover_tests(test_router   tests/unit/test_router.c)
cerv_discover_tests(test_request  tests/unit/test_request.c)
cerv_discover_tests(test_arena    tests/unit/test_arena.c)
cerv_discover_tests(test_response tests/unit/test_response.c)
cerv_discover_tests(test_chan     tests/unit/test_chan.c)
