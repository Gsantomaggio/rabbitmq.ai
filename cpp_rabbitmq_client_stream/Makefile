# Makefile for the C++ RabbitMQ Stream client.
# All real work is delegated to CMake under build/.

BUILD_DIR ?= build
CMAKE     ?= cmake
CTEST     ?= ctest
JOBS      ?= 4
GENERATOR ?=

ifeq ($(GENERATOR),)
CMAKE_FLAGS :=
else
CMAKE_FLAGS := -G $(GENERATOR)
endif

.PHONY: all configure build test it format clean help

all: build

help:
	@echo "Targets:"
	@echo "  build    Configure (if needed) and build the static library + tests."
	@echo "  test     Build and run the unit-test binary (no broker required)."
	@echo "  it       Build and run the integration-test binary (broker on localhost:5552)."
	@echo "  format   Run clang-format -i over include/, src/, tests/."
	@echo "  clean    Remove the build directory."

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure -R unit_tests

it: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure -R integration_tests

format:
	@if command -v clang-format >/dev/null 2>&1; then \
		find include src tests -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' \) \
			-print0 | xargs -0 clang-format -i; \
		echo "clang-format applied."; \
	else \
		echo "clang-format not found, skipping. Install via 'brew install clang-format' or your package manager."; \
	fi

clean:
	rm -rf $(BUILD_DIR)
