cmake_minimum_required(VERSION 3.16)
project(WinZigCParser VERSION 1.0.0)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Git hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
)

# --- Third-party dependencies ---
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(CLI11)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Libraries ---
# Public headers live under include/ (e.g. include/common/, include/utils/, …)

add_library(common INTERFACE)
target_include_directories(common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(winzig_log src/utils/logger.cpp)
target_include_directories(winzig_log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(winzig_log PUBLIC common spdlog::spdlog_header_only)

add_library(tree src/utils/tree.cpp)
target_include_directories(tree PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(tokenizer src/tokenizer/tokenizer.cpp)
target_include_directories(tokenizer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(tokenizer PUBLIC common winzig_log)

add_library(parser src/parser/parser.cpp)
target_include_directories(parser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(parser PUBLIC common winzig_log tree tokenizer)

add_library(semantic_analyzer src/semantic_analyzer/analyzer.cpp src/semantic_analyzer/symbol_table.cpp)
target_include_directories(semantic_analyzer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(semantic_analyzer PUBLIC common winzig_log tree tokenizer parser)

add_library(code_generator src/code_generator/generator.cpp)
target_include_directories(code_generator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(code_generator PUBLIC common winzig_log tree tokenizer parser semantic_analyzer)
add_library(utils
    src/utils/argparser.cpp
    src/utils/filereader.cpp
)
target_include_directories(utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)
target_link_libraries(utils PUBLIC common winzig_log CLI11::CLI11)

# --- Executables ---
add_executable(winzigc app/main.cpp)
target_link_libraries(winzigc PRIVATE utils code_generator)

add_executable(tests
    tests/unit/test_argparser.cpp
    tests/unit/test_parser.cpp
    tests/unit/test_integration.cpp
    tests/unit/test_tokenizer.cpp
    tests/unit/test_filereader.cpp
    tests/unit/test_tree.cpp
    tests/unit/test_result.cpp
    tests/unit/test_logger.cpp
    tests/unit/test_parser_edges.cpp
    tests/unit/test_analyzer.cpp
    tests/unit/test_symbol_table.cpp
)
target_compile_definitions(tests PRIVATE
    TEST_GRAMMAR_DIR="${CMAKE_SOURCE_DIR}/tests/grammar"
    TEST_INTEGRATION_DIR="${CMAKE_SOURCE_DIR}/tests/integration"
)
target_link_libraries(tests PRIVATE gtest_main utils semantic_analyzer)

include(GoogleTest)
gtest_discover_tests(tests)

# --- Coverage ---
option(ENABLE_COVERAGE "Enable coverage" OFF)

if(ENABLE_COVERAGE)
    set(_coverage_targets winzigc utils winzig_log tree tokenizer parser semantic_analyzer code_generator tests)
    foreach(tgt IN LISTS _coverage_targets)
        target_compile_options(${tgt} PRIVATE --coverage)
        target_link_options(${tgt} PRIVATE --coverage)
    endforeach()
endif()
