CMAKE ?= cmake
CMAKE_GENERATOR ?= Ninja
BUILD_DIR ?= build
TEST_BUILD_DIR ?= build-tests

.DEFAULT_GOAL := build

.PHONY: help configure build configure-tests build-tests test \
	test-semantic llvm-poc native-poc clean

help:
	@echo "Flecha build targets:"
	@echo "  make build          Configure and build Flecha"
	@echo "  make test           Configure, build, and run all tests"
	@echo "  make test-semantic  Run only semantic and scope tests"
	@echo "  make llvm-poc       Emit and execute the LLVM proof of concept"
	@echo "  make native-poc     Build and execute a native Flecha program"
	@echo "  make clean          Clean generated build artifacts"
	@echo ""
	@echo "Overrides: CMAKE, CMAKE_GENERATOR, BUILD_DIR, TEST_BUILD_DIR"

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -G "$(CMAKE_GENERATOR)" \
		-DBUILD_TESTING=OFF \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

configure-tests:
	$(CMAKE) -S . -B $(TEST_BUILD_DIR) -G "$(CMAKE_GENERATOR)" \
		-DBUILD_TESTING=ON \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build-tests: configure-tests
	$(CMAKE) --build $(TEST_BUILD_DIR) --parallel

test: build-tests
	$(CMAKE) --build $(TEST_BUILD_DIR) --target test

test-semantic: build-tests
	$(TEST_BUILD_DIR)/src/tests/test_flecha \
		--gtest_filter='Semantic*:*ScopeTests*'

llvm-poc: build
	$(BUILD_DIR)/flecha --emit-llvm examples/llvm_poc.fl \
		-o $(BUILD_DIR)/llvm_poc.ll
	@lli $(BUILD_DIR)/llvm_poc.ll; result=$$?; \
		test $$result -eq 42
	@echo "LLVM proof of concept returned 42"

native-poc: build
	$(BUILD_DIR)/flecha build examples/llvm_poc.fl \
		-o $(BUILD_DIR)/llvm_poc
	@test -f $(BUILD_DIR)/llvm_poc.o
	@$(BUILD_DIR)/llvm_poc; result=$$?; test $$result -eq 42
	@echo "Native proof of concept returned 42"

clean:
	@if test -f $(BUILD_DIR)/CMakeCache.txt; then \
		$(CMAKE) --build $(BUILD_DIR) --target clean; \
	fi
	@if test -f $(TEST_BUILD_DIR)/CMakeCache.txt; then \
		$(CMAKE) --build $(TEST_BUILD_DIR) --target clean; \
	fi
