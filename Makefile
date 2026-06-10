.PHONY: configure build test lint fmt fmt-check tidy pre-commit clean

configure:
	cmake --preset debug

build: configure
	cmake --build --preset debug

test: build
	ctest --preset debug --output-on-failure

FMT_FILES = $(shell find core examples -name '*.hpp' -o -name '*.cpp' 2>/dev/null)

fmt:
	clang-format -i $(FMT_FILES)

fmt-check:
	clang-format --dry-run --Werror $(FMT_FILES)

tidy: configure
	run-clang-tidy -p build/debug -quiet $(FMT_FILES)

lint: fmt-check tidy

pre-commit: lint test

clean:
	rm -rf build
