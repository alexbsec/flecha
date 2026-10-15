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
