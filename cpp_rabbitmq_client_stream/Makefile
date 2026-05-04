BUILD_DIR := build

.PHONY: all build test integration-test format clean

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(BUILD_DIR) --parallel

test: build
	$(BUILD_DIR)/test_unit

integration-test: build
	$(BUILD_DIR)/test_integration

format:
	find src include tests -name '*.cpp' -o -name '*.hpp' | \
	    xargs clang-format -i --style=file 2>/dev/null || \
	    xargs clang-format -i --style="{BasedOnStyle: Google, IndentWidth: 4}"

clean:
	rm -rf $(BUILD_DIR)
