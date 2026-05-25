.PHONY: build clean test test-all

TEST_FILE := $(filter-out test, $(MAKECMDGOALS))
TEST_NAME := $(basename $(notdir $(TEST_FILE)))

build:
	cmake -B build -DCMAKE_TOOLCHAIN_FILE=$$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
	cmake --build build
	@ln -sf build/compile_commands.json compile_commands.json

clean:
	rm -rf build

test: build
	ctest --test-dir build -V -R $(TEST_NAME)

test-all: build
	ctest --test-dir build -V

tests/unit/%:
	@:
