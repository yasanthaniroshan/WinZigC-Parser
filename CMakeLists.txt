cmake_minimum_required(VERSION 3.16)
project(WinZigCParser)

set(CMAKE_CXX_STANDARD 17)

# To install libraries
include(FetchContent)

# create library for utils
add_library(utils 
    src/utils/logger.cpp
    src/utils/argparser.cpp
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