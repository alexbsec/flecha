# Runtime arrays and the first standard-library container

Runtime-sized storage uses `new<[]T>(count)` in an unsafe context. `count` has type
`usize` and is evaluated exactly once, before attempting allocation. The result is
`(*T, error)`, matching the existing pointer/error convention. Numeric, boolean,
and character elements are currently supported; every element starts at zero.
Structures, strings, pointer elements, and partially initialized storage are not
supported by this first runtime-array form.

```flecha
unsafe {
    buffer, err := new<[]i32>(100);
    if err != none { return 1; }
    defer free(buffer);
    buffer[42] = 42;
}
```

The allocator records the element count. Multiplication overflow or allocation
failure returns null and `AllocationError.OutOfMemory`. A zero count can succeed
with a non-null, releasable handle; no element may be accessed. Allocation metadata
is private to the runtime and does not change the raw-pointer representation.
`const` controls mutation as before: `new<[]const i32>(count)` produces `*const i32`.

Raw `pointer[index]` requires unsafe permission and a base pointer from Flecha's
allocator. The index has type `usize` and is checked against the recorded count
before forming an element address. Out-of-bounds access traps. Known stack and
interior addresses, known null pointers, and tracked freed/transferred aliases are
rejected. Arbitrary external pointer provenance remains an unsafe caller obligation:
indexing a foreign allocation or interior address cannot read Flecha's metadata.
This does not add pointer arithmetic, checked references into the heap, or raw-to-
slice conversions. Ordinary raw dereference `*pointer` retains its unsafe validity
obligations, including requiring a live element for a zero-capacity allocation.

`free(pointer)` releases the complete buffer. The supported element types need no
individual destruction. Ordinary `new<T>(value)` still allocates one value and
runs typed destruction, including `tear`; its initializer still runs only after
allocation succeeds. Runtime-array counts, unlike initializers, must run even
when allocation subsequently fails.

## Replacing an owning field

`take(self->field)` transfers an owning raw pointer out of a mutable field and
sets that field to null. It is unsafe and currently limited to owning fields of
the current method receiver. It is a contextual intrinsic: a user-declared
`take` in scope takes precedence. The extracted pointer must be freed or transferred.
Taking a null field returns null. `defer take(...)` is rejected because it would
discard the extracted ownership.

A method may assign a new owner only when the field is known null, including after
`take`. Replacing a live owner is rejected. Aliases from before extraction or
transfer become unavailable. Pending deferred uses prevent conflicting extraction.
Mutable calls accepting a reference to the owning structure conservatively
invalidate tracked aliases to its owning fields; aliases must be obtained again
after the call. General field-effect summaries are not yet implemented.

Construction and destruction rules remain unchanged: `tear` is compiler-invoked,
and an unsafe tear makes a structure immovable and noncopyable.

## Standard-library I32Array

The implementation is Flecha source in `std/src/containers/array_i32.fl`. A project
can declare a local `std` dependency in `longbow.toml`, then import:

```flecha
@imports :: lib::std::containers::array_i32;
```

Construct `array_i32::I32Array::init()` in an unsafe context. This is a concrete
`i32` container, not a generic `Array<T>`. It maintains a used length and an
allocated capacity, starts empty, grows from four slots by doubling, and releases
its buffer through `tear`.

| Method | Result and behavior |
| --- | --- |
| `len()` | Used element count, `usize` |
| `capacity()` | Allocated element count, `usize` |
| `reserve(capacity)` | `error`; increases capacity if needed |
| `push(value)` | `error`; appends, growing if necessary |
| `pop()` | `(i32, error)`; removes the last value or returns `ArrayError.Empty` |
| `get(index)` | `(i32, error)`; returns a value or `ArrayError.IndexOutOfBounds` |
| `set(index, value)` | `error`; replaces an existing element or reports bounds |
| `clear()` | Empties the used prefix while retaining capacity |

Allocation, growth, element-access, and removal methods are unsafe in this initial
container. `len`, `capacity`, and `clear` use checked receiver references. Elements
are returned by value; the API exposes no raw buffer pointer or borrowed element
reference. Bounds errors leave the array unchanged. Growth allocates and copies
before extracting the old buffer, so allocation failure preserves length,
capacity, and contents. Overflow is reported as `AllocationError.OutOfMemory`.

See `examples/dynamic_array` for a Longbow project that grows to 100 integers,
prints element 42, and exits with status 42. `StandardLibraryTests.DynamicArrayI32`
checks 1,000 elements, injected growth failure, overflow, bounds, mutation,
clearing, independent arrays, and exact cleanup of all buffers.
