# Raw allocation: new and free

`new<T>(value)` and `free(pointer)` use the process-wide system allocation
service. Both require an unsafe function or an explicit unsafe block. There is
no public allocator handle and no automatic free at the end of an unsafe block.

```flecha
unsafe {
    (*u8 pointer, error err) = new<u8>(42);
    if err != none { return 1; }
    defer free(pointer);
    *pointer = 43;
}
```

Inferred destructuring is `pointer, err := new<u8>(42);`. The result is a tuple
`(*u8, error)`, so `pointer := new<u8>(42);` binds the entire tuple, not just the
pointer. The existing mixed form `(pointer, error err) = new<u8>(42);` also works
when pointer has already been declared.

## Allocation and evaluation

The compiler computes T's native size and alignment. The runtime allocates
zeroed storage with that alignment. A successful allocation then evaluates the
initializer exactly once and constructs T directly in that storage. This
preserves the addresses of immovable structures, including fresh factory
results and structures nested in arrays or tuples.

Success returns a non-null pointer and `none`. Failure returns null and the
built-in `AllocationError.OutOfMemory` value converted to generic `error`.
Allocation failure **does not evaluate the initializer**, call constructors, or
run teardown. Assignments and reference-handle transfers inside the initializer
are therefore conditional; definite-assignment and borrow checking account for
the failure path. The checker does not yet correlate `err == none` with those
initializer side effects, so initialize needed locals before allocation.

```flecha
unsafe {
    (*Box box, error err) = new<Box>(Box::init(42));
    if err == AllocationError.OutOfMemory { return 1; }
    defer free(box);
    box->setY(10);
}
```

`AllocationError` is a built-in named error family with the single failure
variant `OutOfMemory`; `none` means success as usual. The names `new`, `free`, and
`AllocationError` are declared by the compiler. `new<T>` is built-in typed syntax,
not general user-defined generics.

The error reports failure to obtain the outer allocation. Failures inside an
initializer retain their existing behavior: for example, owning-string runtime
allocation failure terminates the process, and bounds traps do not unwind.
There is no constructor-exception or partial-construction protocol.

## Supported values

Allocatable types are sized owned values: scalars, errors, raw pointers, owning
strings, structures, supported fixed arrays, and tuples. Const-qualified values
are allowed and produce pointers with read-only targets. References, slices,
`stringv`, void, and otherwise unsupported compound types are rejected.

`new<T>(none)` performs defined zero initialization where T already supports
`none`. Structures, including aggregates containing them, require explicit
construction such as `new<Box>(Box::init(...))`. Raw pointer elements are
initialized to null by `none`; they do not allocate their targets.

Existing copy rules apply to an initializer naming an existing value: owning
fields are deep-copied and noncopyable structures must instead be freshly
constructed. New never implicitly moves an existing noncopyable object.
Zero-sized values receive a distinct live allocation that must still be freed.

## Release and deferred release

`free(pointer)` evaluates its pointer argument once. For a non-null pointer it
invokes the structure's tear, destroys owned fields/elements in reverse order,
then releases the backing storage. Direct calls to tear remain forbidden. A
pointer to const T may be freed; target constness does not remove the cleanup
obligation. `free(null)` and `free(none)` do nothing. Void pointers cannot be
freed through this typed API because teardown needs the pointee type.

Free does not reset the caller's variable or invalidate aliases in compiler
analysis yet. It must receive null or the exact live pointer returned by a
matching new allocation, with its original pointee type (optionally const).
Stack addresses, interior pointers, foreign allocations, and already-freed
pointers violate this unsafe contract. Releasing a raw-pointer field or element
does not recursively free the object it points to; only owned values are torn
down.

`defer free(pointer);` captures the pointer value at registration and runs on
scope exit using the existing defer rules. It does not consume the allocation
immediately, and rebinding the original variable does not change the captured
pointer. `defer new<T>(...)` is rejected because it would discard the allocation.
An ordinary ignored allocation result is not automatically freed.

This milestone does not prove allocation ownership, enforce null checks, track
raw aliases, or diagnose all leaks, double frees, and use-after-free. Resource
identity and cross-function consuming-effect analysis remain separate work.
Safe heap owners and dynamic collections are also future features.

## Diagnostics and verification

`FLECHA-S0047` diagnoses unsupported allocation/free types and invalid built-in
usage. Existing argument, unsafe-context, const, copy, definite-assignment, and
borrow diagnostics continue to apply.

`NativeSemanticTests.RawAllocation` covers scalar/string/structure/array/tuple
allocation, const and zero-sized objects, stable construction addresses,
destruction order, null free, and deferred release on control-flow exits. Its
native harness uses linker wrapping to inject one allocator failure without a
production runtime test hook. Runtime tests check zeroing, alignment, overflow,
and invalid alignment. ASan/UBSan and native leak checks cover this fixture.
See [runtime_abi.md](runtime_abi.md) for the storage ABI and
[../examples/raw_allocation.fl](../examples/raw_allocation.fl) for a runnable
example returning 42.
