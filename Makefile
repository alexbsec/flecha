CMAKE ?= cmake
CMAKE_GENERATOR ?= Ninja
BUILD_DIR ?= build
TEST_BUILD_DIR ?= build-tests
SANITIZER_BUILD_DIR ?= build-sanitizers
GTEST_SOURCE_DIR ?=

.DEFAULT_GOAL := build

.PHONY: help configure build configure-tests build-tests test \
	test-semantic llvm-poc native-poc native-if-also native-functions \
	native-while native-loop-control native-casts native-string-view \
	native-string-view-operations native-owning-string native-stdout \
	native-definite-assignment test-neovim test-sanitizers clean

help:
	@echo "Flecha build targets:"
	@echo "  make build          Configure and build Flecha"
	@echo "  make test           Configure, build, and run all tests"
	@echo "  make test-semantic  Run only semantic and scope tests"
	@echo "  make test-neovim    Test Neovim filetype and syntax support"
	@echo "  make test-sanitizers Run unit tests with ASan and UBSan"
	@echo "  make llvm-poc       Emit and execute the LLVM proof of concept"
	@echo "  make native-poc     Build and execute a native Flecha program"
	@echo "  make native-if-also Build and test the if/also native example"
	@echo "  make native-functions Build and test calls and recursion"
	@echo "  make native-while   Build and test while/also loops"
	@echo "  make native-loop-control Build and test loop control"
	@echo "  make native-casts   Build and test numeric casts"
	@echo "  make native-string-view Build and test borrowed strings"
	@echo "  make native-string-view-operations Build and test stringv operations"
	@echo "  make native-owning-string Build and test owning strings"
	@echo "  make native-stdout  Build and test standard output"
	@echo "  make native-definite-assignment Build and test delayed initialization"
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

test-neovim:
	NVIM_LOG_FILE=/dev/null nvim --headless -u NONE -i NONE -n \
		--cmd 'set runtimepath^=$(CURDIR)/editors/neovim' \
		-S editors/neovim/tests/syntax_test.vim

test-sanitizers:
	$(CMAKE) -S . -B $(SANITIZER_BUILD_DIR) -G "$(CMAKE_GENERATOR)" \
		-DBUILD_TESTING=ON \
		-DFLECHA_ENABLE_SANITIZERS=ON \
		$(if $(GTEST_SOURCE_DIR),-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$(GTEST_SOURCE_DIR),) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	$(CMAKE) --build $(SANITIZER_BUILD_DIR) --parallel
	ASAN_OPTIONS=detect_leaks=0 \
	UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		$(SANITIZER_BUILD_DIR)/src/tests/test_flecha
	ASAN_OPTIONS=detect_leaks=1 \
	UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		ctest --test-dir $(SANITIZER_BUILD_DIR) --output-on-failure \
		-R '^NativeSemanticTests.(ControlFlowDefiniteAssignment|ConstDeclarations|UnsafePointers|CheckedReferences|FixedArrays|Iteration|CheckedSlices|Defer|Tuples|ErrorValues)$$'

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

native-if-also: build
	$(BUILD_DIR)/flecha build examples/if_also.fl \
		-o $(BUILD_DIR)/if_also
	@test -f $(BUILD_DIR)/if_also.o
	@$(BUILD_DIR)/if_also; result=$$?; test $$result -eq 42
	@echo "Native if/also example returned 42"

native-functions: build
	$(BUILD_DIR)/flecha build examples/native_functions.fl \
		-o $(BUILD_DIR)/native_functions
	@test -f $(BUILD_DIR)/native_functions.o
	@$(BUILD_DIR)/native_functions; result=$$?; test $$result -eq 42
	@echo "Native function example returned 42"

native-while: build
	$(BUILD_DIR)/flecha build examples/while_also.fl \
		-o $(BUILD_DIR)/while_also
	@test -f $(BUILD_DIR)/while_also.o
	@$(BUILD_DIR)/while_also; result=$$?; test $$result -eq 42
	@echo "Native while/also example returned 42"

native-loop-control: build
	$(BUILD_DIR)/flecha build examples/loop_control.fl \
		-o $(BUILD_DIR)/loop_control
	@test -f $(BUILD_DIR)/loop_control.o
	@$(BUILD_DIR)/loop_control; result=$$?; test $$result -eq 42
	@echo "Native loop-control example returned 42"

native-casts: build
	$(BUILD_DIR)/flecha build examples/numeric_casts.fl \
		-o $(BUILD_DIR)/numeric_casts
	@test -f $(BUILD_DIR)/numeric_casts.o
	@$(BUILD_DIR)/numeric_casts; result=$$?; test $$result -eq 42
	@echo "Native numeric-cast example returned 42"

native-string-view: build
	$(BUILD_DIR)/flecha build examples/string_view.fl \
		-o $(BUILD_DIR)/string_view
	@test -f $(BUILD_DIR)/string_view.o
	@$(BUILD_DIR)/string_view; result=$$?; test $$result -eq 42
	@echo "Native string-view example returned 42"

native-string-view-operations: build
	$(BUILD_DIR)/flecha build src/tests/fixtures/string_view_operations.fl \
		-o $(BUILD_DIR)/string_view_operations
	@test -f $(BUILD_DIR)/string_view_operations.o
	@$(BUILD_DIR)/string_view_operations > \
		$(BUILD_DIR)/string_view_operations.actual; result=$$?; \
		output=`base64 -w 0 $(BUILD_DIR)/string_view_operations.actual`; \
		test "$$output" = "NgpmCmEKbGVjCg=="; \
		test $$result -eq 42
	@echo "Native string-view operations printed expected text and returned 42"

native-owning-string: build
	$(BUILD_DIR)/flecha build src/tests/fixtures/owning_string.fl \
		-o $(BUILD_DIR)/owning_string
	@test -f $(BUILD_DIR)/owning_string.o
	@$(BUILD_DIR)/owning_string > $(BUILD_DIR)/owning_string.actual; \
		result=$$?; \
		output=`base64 -w 0 $(BUILD_DIR)/owning_string.actual`; \
		test "$$output" = "aGkgbW9tCjQyCg=="; \
		test $$result -eq 42
	@echo "Native owning-string example printed expected text and returned 42"

native-stdout: build
	$(BUILD_DIR)/flecha build src/tests/fixtures/stdout.fl \
		-o $(BUILD_DIR)/stdout
	@test -f $(BUILD_DIR)/stdout.o
	@$(BUILD_DIR)/stdout > $(BUILD_DIR)/stdout.actual; result=$$?; \
		output=`base64 -w 0 $(BUILD_DIR)/stdout.actual`; \
		test "$$output" = \
		"RmxlY2hhCjgKMTYKMzIKNjQKNzIKOAoxNgozMgo2NAo3MgoxLjI1CjEuNQp0cnVlCmZhbHNlCiEK"; \
		test $$result -eq 42
	@echo "Native stdout example printed the expected text and returned 42"

native-definite-assignment: build
	$(BUILD_DIR)/flecha build src/tests/fixtures/definite_assignment.fl \
		-o $(BUILD_DIR)/definite_assignment
	@output=`$(BUILD_DIR)/definite_assignment`; result=$$?; \
		test $$result -eq 42; test "$$output" = "initialized"
	@echo "Native definite-assignment example returned 42"

clean:
	@if test -f $(BUILD_DIR)/CMakeCache.txt; then \
		$(CMAKE) --build $(BUILD_DIR) --target clean; \
	fi
	@if test -f $(TEST_BUILD_DIR)/CMakeCache.txt; then \
		$(CMAKE) --build $(TEST_BUILD_DIR) --target clean; \
	fi
