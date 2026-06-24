# Standalone build (no CMake). Requires a C++17 compiler (g++ or clang++).
# Vendored headers: third_party/CLI11, third_party/spdlog (see third_party/README.md).
#
#   make          -> ./winzigc
#   make clean
#   make vendor   -> one-time fetch of third_party/ (only if missing; needs git + network)

CXX         ?= g++
BUILD_DIR   ?= build
OBJ_DIR     := $(BUILD_DIR)/obj
GEN_DIR     := $(BUILD_DIR)/generated

PROJECT_VERSION := 1.0.0
GIT_HASH        := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)
BUILD_TIME      := $(shell date '+%Y-%m-%d %H:%M:%S')
COMPILER_LINE   := $(shell $(CXX) --version 2>/dev/null | head -1)
PLATFORM_NAME   := $(shell uname -s 2>/dev/null || echo unknown)

INC := -Iinclude \
       -I$(GEN_DIR) \
       -Ithird_party/CLI11/include \
       -Ithird_party/spdlog/include

# spdlog is used header-only (no libspdlog link); see third_party/README.md
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 $(INC) -DSPDLOG_HEADER_ONLY=1
LDFLAGS  :=

WINZIGC_SRCS := \
	app/main.cpp \
	src/utils/logger.cpp \
	src/utils/tree.cpp \
	src/utils/argparser.cpp \
	src/utils/filereader.cpp \
	src/tokenizer/tokenizer.cpp \
	src/parser/parser.cpp

WINZIGC_OBJS := $(WINZIGC_SRCS:%.cpp=$(OBJ_DIR)/%.o)

.PHONY: all clean vendor winzigc check-deps

all: winzigc

winzigc: check-deps $(GEN_DIR)/version.h $(WINZIGC_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(WINZIGC_OBJS)
	@chmod +x $@

check-deps:
	@test -f third_party/CLI11/include/CLI/CLI.hpp || \
		(echo "error: missing third_party/CLI11 — run: make vendor" >&2; exit 1)
	@test -f third_party/spdlog/include/spdlog/spdlog.h || \
		(echo "error: missing third_party/spdlog — run: make vendor" >&2; exit 1)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(GEN_DIR)/version.h: include/cmake/version.h.in | $(GEN_DIR)
	@sed \
		-e 's/@PROJECT_VERSION@/$(PROJECT_VERSION)/g' \
		-e 's/@PROJECT_VERSION_MAJOR@/0/g' \
		-e 's/@PROJECT_VERSION_MINOR@/1/g' \
		-e 's/@PROJECT_VERSION_PATCH@/1/g' \
		-e 's/@GIT_HASH@/$(GIT_HASH)/g' \
		-e 's|@CMAKE_CXX_COMPILER_ID@ @CMAKE_CXX_COMPILER_VERSION@|$(COMPILER_LINE)|g' \
		-e 's/@CMAKE_SYSTEM_NAME@/$(PLATFORM_NAME)/g' \
		-e 's/@BUILD_TIME@/$(BUILD_TIME)/g' \
		$< > $@

$(GEN_DIR):
	@mkdir -p $(GEN_DIR)

# Populate third_party/ once (for developers packaging a tarball without .git subdirs).
vendor:
	@./scripts/vendor_deps.sh

clean:
	rm -rf $(OBJ_DIR) $(GEN_DIR) ./winzigc

# Removes CMake artifacts too (optional; use if you previously used cmake -B build).
clean-all: clean
	rm -rf $(BUILD_DIR)/winzigc $(BUILD_DIR)/tests $(BUILD_DIR)/CMakeCache.txt \
		$(BUILD_DIR)/CMakeFiles $(BUILD_DIR)/cmake_install.cmake \
		$(BUILD_DIR)/Makefile $(BUILD_DIR)/_deps 2>/dev/null || true
