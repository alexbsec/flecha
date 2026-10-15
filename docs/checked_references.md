# Checked stack references

Checked references are implemented for initialized scalar and raw-pointer
storage. They borrow storage without allocating, owning, or extending its
lifetime. See [the runnable example](../examples/checked_references.fl).

```flecha
u8 double_local() {
    u8 value = 3;
    ref := &value;
    *ref = *ref * 2;
    return value; // 6; the borrow ended after ref's last use.
}
```

`&value` creates a reference in safe or unsafe code. `*ref` reads its target;
`*ref = value` writes a mutable target. `ref = &other` rebinds a compatible handle,
without writing the old target. Parentheses preserve these meanings.

## Qualifiers and handle movement

| Type | Binding | Target access | Assignment to another handle |
| --- | --- | --- | --- |
| `&T` | Rebindable | Exclusive, writable | Moves |
| `&const T` | Rebindable | Shared, read-only | Copies |
| `constref T` / `const &T` | Fixed | Exclusive, writable | Moving is rejected; reborrow instead |
| `constref const T` / `const &const T` | Fixed | Shared, read-only | Copies |

A reference always requires an initializer; neither `null` nor `none` is a
reference value. The borrowed object must already be initialized. `ref := &value`
infers a read-only target if `value` is const. An expected `&const T` type also
allows `&value` to borrow mutable storage read-only. Already typed references
must match on assignment; write `&*mutableRef` in a shared-reference context to
make an explicit shared reborrow. Cross-type casts and reference comparisons
are rejected.

```flecha
void increment(&u8 value) { *value = *value + 1; }
u8 read(&const u8 value) { return *value; }

u8 example() {
    u8 value = 3;
    ref := &value;
    moved := ref;
    // *ref = 4; // error: moved handle
    increment(moved); // A call reborrows; it does not move moved.
    return read(moved); // A shared call reborrow of a mutable handle is allowed.
}
```

A moved handle becomes usable again after reassignment. Call reborrows last
through the call, including evaluation of later arguments. Arguments evaluate
left to right. Two shared arguments may borrow the same object; overlapping
mutable arguments are rejected. A callee may use or rebind its local parameter
handle, but cannot return a reference or store it in escaping reference storage.

## Lifetimes and alias checking

A mutable borrow prevents conflicting direct reads, writes, and other borrows
of its owner. Shared borrows permit reads and other shared borrows, but prevent
writes and mutable borrows. These checks apply inside unsafe blocks too; unsafe
raw-pointee operations still retain the caller's separate validity obligations.

`child := &*parent` reborrows through a reference. A live mutable child prevents
access through its parent; any live child prevents moving or rebinding its
parent. After the child's last possible use, its parent becomes usable again.
Moving a child handle preserves these restrictions.

The compiler uses the same reachable control-flow graph as definite assignment.
Backward liveness identifies last possible uses; forward may-analysis tracks
possible borrow origins and moved handles across branches, short-circuit
expressions, loops, `break`, `proceed`, and `jumpto`. A use is rejected if a handle
may have moved on any incoming path. Re-executing a declaration cannot reset
storage that is still borrowed. Unreachable code contributes no borrow effects.

The first version is conservative: an assigned reference's owners and parent
reborrows must have enclosing or equal lexical scopes. References cannot be
returned, even when a caller could in principle prove them valid. It does not
use condition/value correlation to prove paths impossible. Temporary borrows
may remain live through their entire expression. Rebinding or moving the handle
used to address a pending indirect assignment is rejected.

## References to pointers

```flecha
void replace(&*u8 slot, *u8 next) {
    *slot = next; // Safe: accesses the borrowed pointer variable.
}

unsafe void write_target(&*u8 slot) {
    **slot = 42; // Caller must establish raw pointee validity.
}
```

The inner pointer may be null; the outer reference cannot. Borrowing its slot
neither validates the raw pointee nor owns it. `&constptr u8` borrows a fixed
pointer slot read-only. Safe references cannot be created from raw dereferences,
and `&raw ref` / `&raw *ref` cannot expose reference storage as a raw pointer.
References to references, pointers containing reference types, references to
`void`, and direct references to `string` or `stringv` remain rejected. Strings
need integration with their existing owner/view lifetime rules before this
feature can safely expose reference-based mutation.

## Integer literal contexts

Integer literals adopt an expected integer type in declarations, assignments,
returns, and arguments. Arithmetic propagates integer context to literal
operands; a typed integer operand provides context to a neighboring literal.
Signed negative literals, including signed minima, are checked as a whole.
Out-of-range contextual literals produce `FLECHA-S0032`. Unconstrained integer
literals retain the existing `i32` default; already typed values never gain
implicit numeric conversions. Explicit numeric casts retain their prior rules.

## Diagnostics and validation

| Code | Meaning |
| --- | --- |
| `FLECHA-S0027` | Reference declaration requires an initializer |
| `FLECHA-S0028` | Possibly moved handle, or move from a fixed mutable handle |
| `FLECHA-S0029` | Conflicting access, borrow, move, or rebind |
| `FLECHA-S0030` | Reference lifetime escape or unsupported reference escape mechanism |
| `FLECHA-S0031` | Invalid safe-reference creation operand |
| `FLECHA-S0032` | Integer literal outside its contextual type's range |
| `FLECHA-S0024` | Unsupported compound storage type |

Existing assignment, qualifier, initialization, cast, and unsafe-context codes
still apply. `make test` covers semantic flow checks, LLVM verification, and
`NativeSemanticTests.CheckedReferences`, which executes calls, moves, reborrows,
control flow, pointer-slot writes, and integer boundaries. `make test-sanitizers`
also includes this native fixture.

Reference values lower to native addresses. No runtime borrow counter or heap
allocation is introduced. Native callers must supply non-null, aligned, live,
initialized storage and satisfy the exclusive/shared aliasing contract for each
reference parameter. The compiler does not verify arbitrary external callers.
