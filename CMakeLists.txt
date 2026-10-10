cmake_minimum_required(VERSION 3.16)
project(PriceTimeOrderBook VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(ORDERBOOK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(orderbook INTERFACE)
target_include_directories(orderbook INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(orderbook INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(orderbook INTERFACE /W4 /permissive-)
else()
    target_compile_options(orderbook INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(ORDERBOOK_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(orderbook INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(orderbook INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "Sanitizers require GCC or Clang.")
    endif()
endif()

add_executable(orderbook_demo main.cpp)
target_link_libraries(orderbook_demo PRIVATE orderbook)

include(CTest)
if(BUILD_TESTING)
    add_executable(orderbook_tests tests/orderbook_tests.cpp)
    target_link_libraries(orderbook_tests PRIVATE orderbook)
    add_test(NAME orderbook_tests COMMAND orderbook_tests)
endif()

add_library(orderbook_replay_support STATIC tools/replay.cpp)
target_link_libraries(orderbook_replay_support PUBLIC orderbook)

if(BUILD_TESTING)
    add_executable(replay_tests tests/replay_tests.cpp)
    target_link_libraries(replay_tests PRIVATE orderbook_replay_support)
    add_test(NAME replay_tests COMMAND replay_tests)
endif()

add_executable(orderbook_replay tools/replay_main.cpp)
target_link_libraries(orderbook_replay PRIVATE orderbook_replay_support)
if(BUILD_TESTING)
    add_test(NAME replay_cli COMMAND "${CMAKE_COMMAND}"
        "-DREPLAY=$<TARGET_FILE:orderbook_replay>"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_replay.cmake")
endif()

add_library(orderbook_benchmark_support STATIC benchmarks/benchmark.cpp)
target_link_libraries(orderbook_benchmark_support PUBLIC orderbook_replay_support)

if(BUILD_TESTING)
    add_executable(benchmark_tests tests/benchmark_tests.cpp)
    target_link_libraries(benchmark_tests PRIVATE orderbook_benchmark_support)
    add_test(NAME benchmark_tests COMMAND benchmark_tests)
endif()

target_compile_definitions(orderbook_benchmark_support PRIVATE ORDERBOOK_BUILD_TYPE="$<CONFIG>")
add_executable(orderbook_bench benchmarks/benchmark_main.cpp)
target_link_libraries(orderbook_bench PRIVATE orderbook_benchmark_support)
if(BUILD_TESTING)
    add_test(NAME benchmark_smoke COMMAND orderbook_bench --operations 120 --repetitions 1 --seed 42)
endif()
