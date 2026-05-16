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
)
target_include_directories(cerv_core PUBLIC include)
target_compile_options(cerv_core PRIVATE -Wall -Wextra -Wpedantic)

find_package(llhttp CONFIG REQUIRED)
target_link_libraries(cerv_core PUBLIC llhttp::llhttp_static)

enable_testing()

add_executable(test_router tests/unit/test_router.c src/router.c)
add_executable(hello examples/hello.c)
target_link_libraries(hello PRIVATE cerv_core)
target_include_directories(test_router PRIVATE include tests)
add_test(NAME router COMMAND test_router)
