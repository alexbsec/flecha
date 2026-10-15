# Flecha runtime ABI

Generated native objects call a versioned C ABI implemented by the Flecha
runtime archive. The current ABI version is `1`, and every exported symbol is
prefixed with `flecha_runtime_v1_`.

The public declarations live in `src/include/runtime/runtime_abi.h`. The ABI
uses only C fixed-width scalar types, `size_t`, `intptr_t`, `uintptr_t`, and raw
pointers. It does not expose C++ types, exceptions, standard-library objects,
or name mangling.

`stringv` crosses the boundary as two arguments: a pointer to immutable bytes
and an exact byte length. A terminating null byte is neither required nor
included in the length.

The compiler emits one typed output call for each supported Flecha scalar
type. `stdoutln` emits the same typed call followed by
`flecha_runtime_v1_stdout_newline`. Formatting policy therefore belongs to the
runtime rather than LLVM lowering.

Object-only compilation intentionally does not need the runtime archive and
may leave runtime symbols unresolved. Executable builds automatically link the
archive configured with the compiler. A missing archive or linker failure is
reported with a structured `FLECHA-C` diagnostic.

Breaking an existing declaration requires a new ABI version and symbol prefix.
New symbols may be added to version 1 without changing existing declarations.

The runtime also owns dynamic safety checks. Version 1 provides checked
`stringv` byte access and substring bounds validation. Invalid indices print a
runtime diagnostic to standard error and terminate the process rather than
allowing an out-of-bounds memory access.

## Planned owning string ABI

The owning `string` type is specified in [`string.md`](string.md). Its planned
ABI representation contains a mutable data pointer, byte length, and capacity.
Runtime entry points will construct a string from a view, clone it, concatenate
values, and destroy its allocation. Constructors and formatters receive an
explicit output pointer rather than returning the three-field structure by
value. This avoids platform-specific aggregate return conventions and keeps
the compiler/runtime boundary stable.

No C++ standard-library type or exception may cross this boundary. Allocation
failure and invalid dynamic operations must use the runtime failure mechanism.
Object-only compilation may leave owning-string runtime symbols unresolved in
the same way it currently leaves output and bounds-check symbols unresolved.

## Structure values (compiler-private ABI)

Flecha structures use named, non-packed LLVM structures, preserving field order
and native alignment. References/raw pointers to structures use opaque native
pointers. This does not establish a portable C aggregate ABI or expose field
layout inspection to Flecha source.

A function whose result contains a structure (including an array or tuple of
structures) returns LLVM `void` and receives an additional first pointer argument
for its result storage. The caller provides final storage, and the callee
constructs into it. This preserves the address of immovable objects from
construction through tear, including nested factory calls. These results must
not be called from a C harness as ordinary C aggregate returns. Tests expose
scalar wrappers instead.

Copyable structure parameters use LLVM aggregate values. The callee clones
owned fields on entry and cleans up that copy on exit. Noncopyable parameters
must use references or raw pointers. Compiler-generated calls invoke tear with
an address, then recursively destroy fields in reverse order. Existing scalar,
string runtime, and structure-free tuple conventions are unchanged.

## Raw allocation storage ABI

`flecha_runtime_v1_alloc(size_t size, size_t alignment)` returns a pointer to
zeroed storage or null. Alignment must be a nonzero power of two. Size arithmetic
is overflow-checked. Zero-sized requests reserve at least one byte. The runtime
supports over-alignment by retaining the original malloc allocation in a hidden
header before the aligned result; this header is private to the runtime.

`flecha_runtime_v1_free(void* pointer)` releases that storage and accepts null.
Only exact live pointers returned by the matching allocator may be passed to it.
The runtime performs no Flecha teardown: generated code runs typed teardown
before calling free. `new<T>` supplies T's size and alignment from the native
LLVM DataLayout, branches on null, constructs only on success, and packages the
result into `(*T, error)`. Allocation errors use the existing generic error ABI.

The allocation-failure test harness wraps the allocation symbol at link time;
production code has no failure-injection setting or test-only global state.
