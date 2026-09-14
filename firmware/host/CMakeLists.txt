cmake_minimum_required(VERSION 3.20)
project(sp01_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SP01_COMPONENTS ${CMAKE_CURRENT_LIST_DIR}/../esp32-s3/components)

# Controller core plus the derived-truth layer the HMI renders.
add_library(sp01_controller STATIC
    ${SP01_COMPONENTS}/controller/controller.cpp
    ${SP01_COMPONENTS}/controller/controller_explain.cpp
)
target_include_directories(sp01_controller PUBLIC
    ${SP01_COMPONENTS}/controller/include
)

# Rotary position decoding: pure logic, so it is testable off-target.
add_library(sp01_position STATIC
    ${SP01_COMPONENTS}/position/position_decoder.cpp
)
target_include_directories(sp01_position PUBLIC
    ${SP01_COMPONENTS}/position/include
)

# Clock and shift accounting.
add_library(sp01_trace STATIC
    ${SP01_COMPONENTS}/trace/trace.cpp
)
target_include_directories(sp01_trace PUBLIC
    ${SP01_COMPONENTS}/trace/include
    ${SP01_COMPONENTS}/controller/include
)

add_library(sp01_testsource STATIC
    ${SP01_COMPONENTS}/testsource/test_source.cpp
)
target_include_directories(sp01_testsource PUBLIC
    ${SP01_COMPONENTS}/testsource/include
    ${SP01_COMPONENTS}/controller/include
)

add_library(sp01_timekeep STATIC
    ${SP01_COMPONENTS}/timekeep/time_shift.cpp
)
target_include_directories(sp01_timekeep PUBLIC
    ${SP01_COMPONENTS}/timekeep/include
)

add_library(sp01_host_support INTERFACE)
target_include_directories(sp01_host_support INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${SP01_COMPONENTS}/controller/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(lib sp01_controller sp01_position sp01_timekeep sp01_trace sp01_testsource)
        target_compile_options(${lib} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endforeach()
    target_compile_options(sp01_host_support INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(sp01_host src/main.cpp)
target_link_libraries(sp01_host PRIVATE sp01_controller sp01_host_support)

enable_testing()

function(sp01_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE sp01_controller sp01_host_support
                          sp01_position sp01_timekeep sp01_trace sp01_testsource)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sp01_test(sp01_host_tests        tests/controller_test.cpp)
sp01_test(sp01_g3_gate_tests     tests/gate_g3_test.cpp)
sp01_test(sp01_reject_gate_tests tests/gate_reject_test.cpp)
sp01_test(sp01_explain_tests     tests/explain_test.cpp)
sp01_test(sp01_position_tests    tests/position_decoder_test.cpp)
sp01_test(sp01_time_shift_tests  tests/time_shift_test.cpp)
sp01_test(sp01_trace_tests       tests/trace_test.cpp)
sp01_test(sp01_test_source_tests tests/test_source_test.cpp)
