# LLVM code generation proof of concept

The LLVM backend consumes `lowering::BoundProgram`; it does not read syntax
nodes or perform name lookup and type inference. `LlvmCodeGenerator` first
declares every function, then emits bodies so calls to later functions work.
Every generated module is verified with LLVM before it is returned.

The proof of concept supports primitive integer, floating-point, Boolean, and
character values; functions and parameters; stack-backed local variables;
loads, stores, assignments, unary and binary operations; direct calls;
returns; and conditional control flow. Boolean `and` and `or` use branches and
PHI nodes to preserve short-circuit behavior. Integer logical operations remain
ordinary bitwise operations.

Strings, `null`, `none`, pointer dereferencing, object emission, target data
layouts, optimization pipelines, and native linking are deliberately deferred.
Attempting to emit an unsupported semantic type fails code generation with a
message instead of producing invalid LLVM IR.

Use the compiler driver to emit textual LLVM IR:

```sh
make build
build/flecha --emit-llvm examples/llvm_poc.fl -o build/llvm_poc.ll
```

Run the complete proof of concept with:

```sh
make llvm-poc
```

That target emits the example module, executes it with `lli`, and verifies that
the program returns `42`.
