cmake_minimum_required(VERSION 3.25)
project(orderbook VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ORDERBOOK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(orderbook_options INTERFACE)
if(MSVC)
    target_compile_options(orderbook_options INTERFACE /W4 /permissive-)
else()
    target_compile_options(orderbook_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(ORDERBOOK_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(orderbook_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(orderbook_options INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "The sanitizer preset requires GCC or Clang.")
    endif()
endif()

add_library(orderbook_core STATIC src/Orderbook.cpp)
target_include_directories(orderbook_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_features(orderbook_core PUBLIC cxx_std_20)
target_link_libraries(orderbook_core PUBLIC Threads::Threads orderbook_options)

add_executable(orderbook src/main.cpp)
target_link_libraries(orderbook PRIVATE orderbook_core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
