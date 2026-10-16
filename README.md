# Flecha

<p align="center">
  <img src="identity/flecha-logo.png" alt="Flecha logo" width="360">
</p>

Flecha is an early-stage, statically typed systems programming language and compiler. It is designed for predictable execution, explicit memory operations, strong diagnostics, and direct access to native targets without a garbage-collected runtime.

The compiler is implemented in modern C++ and currently includes a tokenizer, parser, semantic analyzer, lowering pipeline, LLVM backend, native compiler, runtime support, standard library, and the Longbow package/project tool.

> **Status:** Flecha is under active development. The language specification and several design documents describe intended or experimental behavior; they are not yet a promise of a stable language or ABI.

## Highlights

- Statically typed primitives, functions, enums with exhaustive matching, tuples, structures, arrays, strings, and error values
- Explicit `unsafe` features for raw pointers and allocation
- Checked references and slices, ownership contracts, and definite-assignment analysis
- Modules and library imports through `root::` and `lib::` paths
- LLVM IR emission and native executable/object generation
- Human-readable source diagnostics
- A small package/project workflow through `longbow`
- Neovim filetype and syntax support

## Quick start

### Requirements

Build requirements are:

- CMake 3.20 or newer
- A C++23 compiler (Clang or GCC are the primary tested choices)
- Ninja, or another CMake generator
- LLVM development libraries and tools

LLVM's `lli` is required for the LLVM proof-of-concept target. Neovim is required only for the editor integration test. Test builds fetch GoogleTest through CMake unless a local checkout is supplied.

### Build the compiler

```sh
make build
```

The main compiler is written to `build/flecha`; the project tool is written to `build/longbow`.

To configure and build directly with CMake:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=OFF
cmake --build build --parallel
```

### Check and build a program

Flecha source files use the `.fl` extension. An executable entry module must define either `i32 main()` or `void main()`.

```sh
build/flecha check examples/factorial.fl
build/flecha build examples/factorial.fl -o build/factorial
./build/factorial
```

The example prints:

```text
Factorial: 120
```

The compiler also supports explicit output stages:

```sh
# Emit textual LLVM IR
build/flecha --emit-llvm examples/llvm_poc.fl -o build/llvm_poc.ll

# Emit a native object file
build/flecha --emit-object examples/llvm_poc.fl -o build/llvm_poc.o
```

The complete command-line form is:

```text
flecha <check|build|--emit-llvm|--emit-object> <input.fl> [-o output]
       [--root source-directory] [--lib name=source-directory ...]
```

## A small Flecha program

```flecha
i32 factorial(i32 n) {
    if n <= 1 {
        return 1;
    }

    return n * factorial(n - 1);
}

void main() {
    result := factorial(5);
    stdout("Factorial: ");
    stdoutln(result);
}
```

Functions place the return type before the function name. Local type inference uses `:=`; explicit declarations are also supported. See [`SPEC.md`](SPEC.md) for the current syntax and design goals.

## Projects and packages with Longbow

Longbow manages a project described by `longbow.toml`.

```sh
# Create a program project
build/longbow new hello
cd hello

# Check, build, or run it from the project directory
../build/longbow check
../build/longbow build
../build/longbow run
```

Create a library with `longbow new NAME --lib`, or initialize an existing empty project directory with `longbow init` (and `--lib` for a library). A minimal manifest looks like this:

```toml
[package]
name = "my_program"
version = "0.1.0"
kind = "program"

[dependencies]
```

Dependencies are local library paths and are resolved from the package's `src` directory:

```toml
[dependencies]
math = { path = "../math" }
```

Longbow's available commands are:

```text
longbow new NAME [--lib]
longbow init [--lib]
longbow check|build|run|clean
longbow run -- [program arguments]
```

See [`docs/modules.md`](docs/modules.md) for module layout, imports, aliases, and library resolution.

## Testing

Configure and run the complete test suite with:

```sh
make test
```

Useful focused targets include:

```sh
make test-semantic       # Semantic and scope tests
make test-neovim         # Neovim syntax/filetype integration
make test-sanitizers     # AddressSanitizer and UBSan tests
make llvm-poc            # Emit and execute LLVM IR; expects exit code 42
make native-poc          # Build and execute a native example; expects 42
make native-if-also
make native-functions
make native-while
make native-loop-control
make native-casts
make native-string-view
make native-owning-string
make native-stdout
make native-definite-assignment
```

Test builds use `build-tests` by default. The sanitizer build uses `build-sanitizers`. These locations can be overridden:

```sh
make BUILD_DIR=out TEST_BUILD_DIR=out-tests build
```

If GoogleTest is already available locally, pass its source directory to the sanitizer target with `GTEST_SOURCE_DIR=/path/to/googletest`.

## Repository layout

```text
src/                    Compiler, runtime, standard library, and tests
std/                    Flecha standard-library source packages
examples/               Small language and package examples
docs/                   Feature, implementation, and design documentation
editors/neovim/         Neovim filetype and syntax support
SPEC.md                 Current language specification draft
CODE_STYLE.md           C++ and repository style guidance
CMakeLists.txt          CMake build definition
Makefile                Convenient build, test, and example targets
```

## Documentation guide

- [`SPEC.md`](SPEC.md) — language goals, syntax, types, and current specification draft
- [`docs/semantic_analysis.md`](docs/semantic_analysis.md) — name binding, types, scopes, and analysis pipeline
- [`docs/llvm_codegen.md`](docs/llvm_codegen.md) — current LLVM backend scope and limitations
- [`docs/modules.md`](docs/modules.md) — modules, imports, and package libraries
- [`docs/memory_model.md`](docs/memory_model.md) — memory model proposal and implementation status
- [`docs/ownership_contracts.md`](docs/ownership_contracts.md) — `own` and `lend` contracts
- [`docs/checked_references.md`](docs/checked_references.md) — checked reference behavior
- [`docs/dynamic_arrays.md`](docs/dynamic_arrays.md) and [`docs/fixed_arrays.md`](docs/fixed_arrays.md) — array features
- [`docs/error_values.md`](docs/error_values.md) — error declarations and propagation
- [`docs/enums.md`](docs/enums.md) — nominal enums, defaults, discriminants, and member functions
- [`editors/neovim/README.md`](editors/neovim/README.md) — Neovim installation and usage

The documentation intentionally labels proposals and proof-of-concept functionality. When behavior differs between the specification and the compiler, compiler tests and diagnostics are the most reliable description of current behavior.

## Development

Use the existing build and test targets when possible. C++ code follows the conventions in [`CODE_STYLE.md`](CODE_STYLE.md), and generated build directories should remain uncommitted. Before opening a change, run the relevant focused tests as well as `make test` when practical.

The working tree may contain experimental language work, so changes should preserve existing diagnostics, examples, and test fixtures unless the behavior change is intentional and documented.

## Contributing

Contributions are welcome. For language changes, start by describing the syntax, typing, diagnostics, runtime/code-generation implications, and compatibility impact. Update the appropriate specification or design document and add examples or regression tests for implemented behavior.

Please read [`CODE_STYLE.md`](CODE_STYLE.md) before changing C++ code. This project is released under the license in [`LICENSE`](LICENSE).

## License

Flecha is distributed under the terms of [`LICENSE`](LICENSE).
