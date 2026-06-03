.PHONY: build clean test test-all

TEST_ARGS  := $(filter-out test, $(MAKECMDGOALS))
TEST_SUITE := $(basename $(notdir $(firstword $(TEST_ARGS))))
TEST_CASE  := $(word 2, $(TEST_ARGS))

build:
	cmake -B build -DCMAKE_TOOLCHAIN_FILE=$$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
	cmake --build build
	@ln -sf build/compile_commands.json compile_commands.json

clean:
	rm -rf build

test: build
	ctest --test-dir build -V $(if $(TEST_SUITE),-R "$(TEST_SUITE)$(if $(TEST_CASE),::$(TEST_CASE))")

tests/unit/%:
	@:

test_%:
	@:
