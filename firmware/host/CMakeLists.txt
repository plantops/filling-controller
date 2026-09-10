cmake_minimum_required(VERSION 3.20)
project(sp01_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sp01_controller STATIC
    ../esp32-s3/components/controller/controller.cpp
)
target_include_directories(sp01_controller PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../esp32-s3/components/controller/include
)

add_library(sp01_host_support INTERFACE)
target_include_directories(sp01_host_support INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../esp32-s3/components/controller/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sp01_controller PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(sp01_host_support INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(sp01_host src/main.cpp)
target_link_libraries(sp01_host PRIVATE sp01_controller sp01_host_support)

enable_testing()
add_executable(sp01_host_tests tests/controller_test.cpp)
target_link_libraries(sp01_host_tests PRIVATE sp01_controller sp01_host_support)
add_test(NAME sp01_host_tests COMMAND sp01_host_tests)

add_executable(sp01_g3_gate_tests tests/gate_g3_test.cpp)
target_link_libraries(sp01_g3_gate_tests PRIVATE sp01_controller sp01_host_support)
add_test(NAME sp01_g3_gate_tests COMMAND sp01_g3_gate_tests)
