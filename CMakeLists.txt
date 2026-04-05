cmake_minimum_required(VERSION 3.16)
project(WinZigCParser)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# To install libraries
include(FetchContent)

# create library for utils
add_library(utils 
    src/utils/logger.cpp
    src/utils/argparser.cpp
    src/utils/filereader.cpp
)

# Download the CLI parser
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
)

FetchContent_MakeAvailable(CLI11)

# Download spdlog logger 
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(spdlog)

# Download Googletest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

# For Windows compatibility (safe to include always)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Include the header files
target_include_directories(utils PUBLIC include)

# Create executable
add_executable(parser app/main.cpp)


# Link libraries
target_link_libraries(utils
    PUBLIC
        CLI11::CLI11
        spdlog::spdlog_header_only
)
target_link_libraries(parser
    PRIVATE
        utils
)

# Add test scripts for testing
add_executable(tests 
    tests/unit/test_argparser.cpp
)

target_link_libraries(tests
    PRIVATE
        gtest_main
        utils
)

include(GoogleTest)
gtest_discover_tests(tests)

# Test Coverage
option(ENABLE_COVERAGE "Enable coverage" ON)

if(ENABLE_COVERAGE)
    target_compile_options(parser PRIVATE --coverage)
    target_link_options(parser PRIVATE --coverage)
    target_compile_options(utils PRIVATE --coverage)
    target_link_options(utils PRIVATE --coverage)
    # tests links utils; instrumented .o files need the same coverage runtime at link time
    target_compile_options(tests PRIVATE --coverage)
    target_link_options(tests PRIVATE --coverage)
endif()