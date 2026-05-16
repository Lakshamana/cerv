.PHONY: build clean test

build:
	cmake -B build -DCMAKE_TOOLCHAIN_FILE=$$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
	cmake --build build
	@ln -sf build/compile_commands.json compile_commands.json

clean:
	rm -rf build

test: build
	ctest --test-dir build -V
