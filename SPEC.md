# Flecha Language Specification

Status: early design draft  
File extension: `.fl`  
Compiler command: `flecha`

## 1. Purpose

Flecha is a low-level, statically typed programming language for software that needs predictable execution, explicit control over memory, and direct access to hardware and operating-system facilities.

The language is intended for systems software such as kernels, embedded programs, runtimes, game engines, databases, command-line tools, and performance-sensitive libraries. It should provide C-like control while reducing accidental undefined behavior and improving type safety, diagnostics, and tooling.

Flecha should make the cost and behavior of code locally visible. Allocation, unsafe memory access, control transfer, and conversions must not be hidden behind surprising language behavior.

## 2. Design goals

Flecha aims to provide:

- deterministic execution without a garbage collector;
- predictable data representation and calling conventions;
- zero-cost abstractions where practical;
- explicit unsafe boundaries;
- strong static typing without a large or academic-looking syntax;
- first-class tuples and multiple return values;
- a zero value for every primitive type;
- direct comparison with the universal `none` literal;
- simple interoperability with C;
- fast compilation and useful diagnostics;
- a small language core that can eventually compile itself.

Flecha does not initially aim to provide:

- classes or implementation inheritance;
- exceptions;
- an implicit garbage-collected runtime;
- hidden heap allocation;
- a sophisticated borrow checker in the first implementation;
- generators, coroutines, or asynchronous functions in the first implementation;
- a stable binary interface between arbitrary Flecha compiler versions.

## 3. Source files and entry points

Flecha source files use the `.fl` extension.

```text
main.fl
```

A hosted executable may use an integer-returning entry point:

```flecha
i32 main() {
    return 0;
}
```

A freestanding environment may provide its own entry point and permit `void`:

```flecha
unsafe void main() {
    return;
}
```

The precise hosted and freestanding entry-point rules remain to be specified.

## 4. Lexical conventions

Identifiers begin with a letter or underscore and may subsequently contain letters, digits, or underscores. Flecha is case-sensitive.

```flecha
value
source_length
CopyError
```

Line and block comments use familiar C-style syntax:

```flecha
// A line comment

/* A block comment */
```

Statements are terminated with semicolons. Blocks are enclosed in braces.

## 5. Functions

Flecha function declarations place modifiers first, followed by the return type, function name, parameters, and body:

```text
[modifiers] ReturnType functionName(parameters) {
    body
}
```

Examples:

```flecha
void reset() {
    return;
}

i32 add(i32 left, i32 right) {
    return left + right;
}

unsafe void write_register(*volatile u32 address, u32 value) {
    *address = value;
    return;
}
```

Tentative function modifiers include:

- `unsafe`
- `export`
- `extern`
- `inline`
- `comptime`

Modifiers and their exact interactions will be specified as their corresponding features are implemented.

## 6. Return and yield

`return` ends a function and optionally supplies its final result:

```flecha
i32 identity(i32 value) {
    return value;
}
```

```flecha
void stop() {
    return;
}
```

`yield` is reserved for a future suspension construct. It will not be an alternative spelling of `return`.

The intended distinction is:

- `return` permanently ends the current function invocation;
- `yield` produces an intermediate value and suspends a generator or coroutine so it can later resume.

Generators and coroutines are outside the initial language core. Reserving `yield` now prevents incompatible syntax later.

## 7. Primitive types

The initial primitive type set is expected to include:

| Category | Types |
| --- | --- |
| Signed integers | `i8`, `i16`, `i32`, `i64`, `isize` |
| Unsigned integers | `u8`, `u16`, `u32`, `u64`, `usize` |
| Floating point | `f32`, `f64` |
| Logical | `bool` |
| Character | `char` |
| No value | `void` |
| Pointer | `*T`, with applicable qualifiers |

Integer sizes are part of their type names. `isize` and `usize` have the native pointer width of the compilation target.

Implicit conversion between distinct primitive types should be avoided. In particular, integers, booleans, and pointers remain differently typed even when their representations or zero values coincide.

## 8. Universal zero and `none`

Every primitive Flecha type has a defined zero value. `none` is a universal, polymorphic zero literal that can be converted to the zero value of a destination or comparison type.

Tentative zero values are:

| Type category | Zero value |
| --- | --- |
| Integer | `0` |
| Floating point | positive zero, `+0.0` |
| `bool` | `false` |
| `char` | the null character, `'\0'` |
| Pointer | `null` |
| Error type | its designated no-error value |

Examples:

```flecha
i32 count = none;       // i32(0)
bool ready = none;      // false
*u8 address = none;     // null pointer
```

After conversion, a value does not retain metadata recording that it originated from the `none` literal:

```flecha
i32 count = none;

count == none;      // true
count === none;     // false
count === i32(0);   // true
```

For primitives, `none` means the zero value rather than a separately representable absence state. Consequently, a primitive integer cannot distinguish “no integer” from integer zero. A future tagged optional type may represent that distinction when required.

### 8.1 Initialization

Ordinary typed declarations may omit an initializer, but the variable must be
assigned on every reachable path before it is read:

```flecha
i32 count;          // not initialized
count = 42;         // now readable
bool ready = none;  // explicitly initialized to false
```

The compiler checks definite assignment through branches, loops, and jumps.
Owning `string` supports delayed initialization; borrowed `stringv` requires
an initializer. See [definite assignment](docs/definite_assignment.md).

### 8.2 Const declarations

`const T name = value;` initializes a fixed binding. Its initializer may be a
runtime expression; `const` does not imply compile-time evaluation. Assignment
to the binding is an error, even if the assigned value is unchanged. A const
local must have an initializer; delayed const initialization is not supported.

```flecha
i32 add(const i32 input) {
    const i32 increment = 2;
    return input + increment;
}
```

Const parameters are initialized by the caller and cannot be reassigned in the
callee. Copying a const value does not make the destination const. A top-level
const qualifier on a return or cast type likewise does not qualify the receiving
binding. Nested target qualifiers remain part of type identity. Existing owning
string copies and borrowed-view lifetime checks remain in force.

The implemented declaration form uses an explicit type; `name := value` still
creates a mutable binding. `constval` is an ordinary identifier, not a keyword.
See [const declarations and compound types](docs/const_types.md).

## 9. Equality

Flecha distinguishes value equality from strict type-and-value equality.

### 9.1 Value equality

`==` and `!=` compare values. When exactly one operand is the literal `none`, `none` is contextually converted to the zero value of the other operand's type.

```flecha
0 == none;       // true
false == none;   // true
null == none;    // true
```

This special treatment does not generally make differently typed concrete values comparable:

```flecha
i32 signed_zero = 0;
u32 unsigned_zero = 0;

signed_zero == unsigned_zero; // compile-time type error
0 == false;                   // compile-time type error
```

The initial equality rule is:

1. If both operands have the same comparable type, compare their values.
2. If exactly one operand has type `none`, convert it to the other operand's zero value and compare.
3. Otherwise, report a type error.

### 9.2 Strict equality

`===` and `!==` compare without contextual conversion. Strict equality succeeds only when both operands have the same type and equal values.

```flecha
0 === none;       // false
false === none;   // false
none === none;    // true
i32(0) === i32(0); // true
i32(0) === u32(0); // false
```

Strict equality is semantic type-and-value equality, not necessarily raw byte equality. Padding, floating-point representations, and other representation details must not silently turn `===` into a memory comparison. A separate explicit operation may be provided for comparing raw representations.

The behavior of floating-point NaNs and signed zero under both equality operators remains to be specified.

## 10. Tuples and multiple return values

Tuples are first-class values. A tuple type is written as a comma-separated type list inside parentheses:

```flecha
(i32, bool)
(usize, CopyError)
```

Functions can return tuples directly:

```flecha
(i32, i32) divide(i32 dividend, i32 divisor) {
    return (dividend / divisor, dividend % divisor);
}
```

Tuple destructuring binds the returned elements:

```flecha
quotient, remainder := divide(10, 3);
```

Explicitly typed destructuring is also supported:

```flecha
i32 quotient, i32 remainder = divide(10, 3);
i32 q, r = divide(10, 3); // One type applies to both names.
```

Tuples, tuple parameters/results, and flat destructuring are implemented.
Binding-list parentheses are optional. Without parentheses, explicit types
carry forward to subsequent names until another type is written. Mixed
bindings such as `(existing, i32 fresh) = value;` and `_` discards are supported.
The right-hand side evaluates once before destination stores. Tuples use value
semantics, including deep copies of owning strings; stored borrowed elements
remain unsupported. The native representation is an LLVM non-packed struct,
with a compiler-internal aggregate calling convention. See
[tuples and destructuring](docs/tuples.md) for syntax, cleanup, and current limits.
Named and generic error values are implemented; see [error_values.md](docs/error_values.md).

## 11. Errors as values

Flecha does not use exceptions in its core error model. Errors are ordinary typed values and may be returned as tuple elements.

```flecha
error CopyError {
    InvalidPointer,
    DestinationTooSmall,
}
```

The no-error state is the error type's zero value and therefore compares equal to `none`:

```flecha
unsafe (usize, CopyError) copy(
    []u8 destination,
    []const u8 source
) {
    if destination.len < source.len {
        return (0, CopyError.DestinationTooSmall);
    }

    memory_copy(destination.ptr, source.ptr, source.len);
    return (source.len, none);
}
```

Callers inspect the error explicitly:

```flecha
unsafe {
    (copied, err) := copy(destination, source);
    if err != none {
        return (0, err);
    }
}
```

Error declarations contain one or more distinct failure names, with an optional
trailing comma. `none` is implicit. Named types only accept their own family or
`none`; built-in `error` accepts any family and preserves its identity. Conversion
from a named error to `error` is implicit, including within tuples. The reverse
conversion and numeric casts are rejected.

`==`/`!=` compare matching named families, or widen a named error when the other
operand is generic `error`. Direct comparison of two different named families is
rejected. `none` is contextually success; all named successes widen to the same
generic success. Strict `===`/`!==` perform no widening or contextual conversion:
different static error types, and error versus literal `none`, are unequal.

Error values support ordinary const, definite-assignment, aggregate, and
borrowing rules. An uninitialized error variable cannot be read. They are
allocation-free and copyable, with no payloads or automatic propagation.
See [error_values.md](docs/error_values.md) for implemented behavior and the
compiler-private representation. The allocator example above still depends on
future allocation APIs.

## 12. Unsafe code

Unsafe operations include behavior the compiler cannot fully validate, such as dereferencing raw pointers, accessing volatile memory, constructing values from unchecked representations, and calling functions with caller-maintained safety requirements.

An `unsafe` function places safety obligations on its caller:

```flecha
unsafe void write_register(*volatile u32 address, u32 value) {
    *address = value;
    return;
}
```

Calling it requires an explicit unsafe context:

```flecha
void configure_device() {
    unsafe {
        write_register(0x40001000 as *volatile u32, 1);
    }
}
```

An ordinary safe function may internally use a small unsafe block while preserving a safe external contract:

```flecha
u32 read_device(*volatile const u32 address) {
    unsafe {
        return *address;
    }
}
```

A function should not be marked `unsafe` merely because its implementation contains an unsafe operation. It should be marked `unsafe` when correct use depends on an invariant the compiler cannot enforce at the call site.

Unsafe functions and `unsafe { ... }` blocks are implemented. An unsafe
function can call another unsafe function directly. Safe callees do not inherit
permission from unsafe callers. The shorthand `unsafe copy(...)` is rejected;
use a block instead. Ordinary nested blocks also create lexical scopes.

Raw address creation is explicit: `&raw value` produces a raw pointer and
requires an unsafe context. `&value` creates a checked reference.
The volatile and integer-address examples above remain future features.
See [unsafe contexts and pointers](docs/unsafe_pointers.md).

## 13. Pointers, arrays, and slices

The intended pointer syntax keeps qualifiers inside the type rather than attaching them to individual declarations:

```flecha
*u8
*const u8
*volatile u32
*volatile const u32
```

The parser and type model represent recursive pointers and references with
const qualifiers at each layer:

| Form | Binding | Target |
| --- | --- | --- |
| `*u8` / `&u8` | Rebindable | Writable |
| `*const u8` / `&const u8` | Rebindable | Read-only |
| `const *u8` / `const &u8` | Fixed | Writable |
| `const *const u8` / `const &const u8` | Fixed | Read-only |

`constptr T` is an alias for `const *T`; `constref T` is an alias for
`const &T`. They normalize to the same types, including when nested. For example,
`&constptr u8` is `&const *u8`. Duplicate const qualifiers on one layer are rejected.
Consecutive reference markers need whitespace (`& &u8`) because `&&` is a logical
operator.

Raw pointer declarations, parameters, returns, null initialization, and equality
are implemented. `&raw value`, raw dereferences, and raw indirect assignments require
an unsafe function or block. Unsafe permission does not override constness.
Pointer values can be copied in safe code. Raw pointers carry no checked lifetime
or non-null guarantee. A pointer may add const to its immediate target, but
cannot remove const or change qualifiers deeper inside a nested target type.

Checked `&T` and `&const T` references to initialized scalar and raw-pointer
storage are implemented. `&value`, `*ref`, and `*ref = value` are safe operations.
Reference assignment rebinds; mutable handle assignment moves, shared handle
assignment copies, and reference arguments reborrow for the call. Mutable
borrows exclude conflicting access; shared borrows exclude mutation. The
compiler tracks last possible uses, borrow origins, and moved handles through
control flow. `constref T` fixes the handle without making its target read-only;
moving a fixed mutable handle is rejected.

References require initializers and cannot contain `none` or `null`. References
to shorter-lived storage cannot escape into outer bindings. Reference returns,
raw/reference conversions, reference comparisons, direct references to strings
or views, and nested reference storage are rejected in this first version.
`&*T` borrows a raw-pointer slot; accessing the raw pointee still requires unsafe
permission. See [checked references](docs/checked_references.md) for exact rules,
limits, and diagnostics. Integer literals gain contextual integer types with
representability checks; this does not convert already typed values.

Pointer casts, arithmetic, ordering, and `volatile` remain future features.

### 13.1 Fixed arrays

Fixed arrays are implemented as inline, contiguous values with length in the type:

```flecha
[3]i32 values = [10, 20, 30];
zeros := [0; 3];
[3]i32 empty_values = none;
values[0] = 42;
const [2]u8 fixed = [1, 2];
element := &values[1];
```

`[N]T` requires a nonnegative integer literal length. List initializers must match
it; repeat initializers evaluate their element once. Arrays copy on assignment,
argument passing, and return. Supported elements are scalars, raw pointers, and
nested arrays. Reference handles and string/view elements remain unsupported.

`.len()` returns `usize`. Indices must be `usize`; integer literals are contextual.
Statically known invalid indices are rejected and other indices have runtime
bounds checks, including inside unsafe blocks. `const [N]T` prevents mutation.
Whole-array and element references use the existing checked borrowing rules;
an element borrow conservatively borrows the entire array. See
[fixed arrays](docs/fixed_arrays.md) for full rules and limitations.

### 13.2 Iteration

```flecha
for index := range values {}
for (index, value) := range values {}
for (u32 index, i32 value) = range values {}
loop usize index = 0; index < values.len(); index++ {}
```

Range evaluates and copies a fixed-array value once, then yields ascending indices
and optional element copies. Inferred indices use `usize`; explicit integer index
types must represent every index. `_` discards a binding. Bindings are local to
iteration. Counted loops execute their initializer once and their update after
each body execution, including `proceed`; `break` skips the update. Postfix `++`
works on writable integer locations, evaluates the location once, and yields its
previous value. See [iteration](docs/iteration.md).

### 13.3 Checked slices

Slices are implemented as borrowed addresses and `usize` lengths:

```flecha
[]i32 mutable_view = values[1:3];
[]const i32 readonly_view = mutable_view[:];
const []i32 fixed_handle = values[:];
```

`start:end` is half-open; omitted bounds default to zero and receiver length.
Slices neither allocate nor own their elements. Bounds are checked even in unsafe
blocks. `len()`, indexing, `front()`, and `back()` are supported; endpoint methods
are writable locations when elements are mutable. Mutable handles move, shared
handles copy, and calls reborrow. Slice ranges take a shared reborrow rather than
copying the backing elements. Slice returns and nested borrowed storage remain
unsupported pending lifetime contracts. See [checked slices](docs/checked_slices.md)
for exact rules, limitations, and diagnostics.

## 14. Memory and runtime model

Flecha should not require a garbage collector. Stack allocation is the ordinary default. Heap allocation must be explicit and should use caller-selected allocators rather than a mandatory hidden global allocator.

The language should avoid hidden allocation in:

- tuple construction;
- error returns;
- string and slice operations;
- generic specialization;
- ordinary function calls.

Owning strings have deterministic lexical destruction. `defer function(args);`
captures arguments at registration and runs the call when its current lexical
scope exits, before that scope destroys its strings. Calls run in reverse
registration order on fallthrough, return, break, proceed, and outward jumps.
Return expressions evaluate before cleanup; traps do not unwind. String/view
captures are independent snapshots, and reference/slice captures retain their
loans until scope exit. Only direct function calls are currently supported. See
[deferred calls](docs/defer.md) for the full contract and examples. General
ownership and allocator cleanup remain future work.

Flecha should support freestanding programs that do not link a language runtime.

## 15. Data layout and interoperability

Flecha should eventually provide:

- predictable struct, tuple, and enum layouts;
- explicit size and alignment inspection;
- packed and explicitly aligned representations;
- volatile memory access;
- native function pointers;
- C-compatible calling conventions;
- importing and exporting C-compatible symbols.

Layout must not be inferred solely from the behavior of a particular backend. Language-level rules will define which layouts are guaranteed and which remain implementation-defined.

## 16. Compilation model

The reference compiler will initially be implemented in C++23.

The proposed bootstrap pipeline is:

```text
Flecha source
    -> lexer
    -> parser
    -> abstract syntax tree
    -> semantic and type analysis
    -> Flecha intermediate representation
    -> C or LLVM backend
    -> object file
    -> native linker
    -> executable
```

The first backend may emit portable C so the frontend and language semantics can be validated quickly. A later LLVM backend can provide native object generation, optimization, debug information, and broad target support.

The compiler should eventually be self-hosted in Flecha, but self-hosting is not an initial milestone.

## 17. Tooling

The intended command-line interface is:

```text
flecha build main.fl
flecha run main.fl
flecha check main.fl
flecha fmt main.fl
flecha test
```

Flecha tooling should eventually include:

- an official formatter;
- an editor language server;
- source-level diagnostics;
- go-to-definition and symbol navigation;
- completion and hover information;
- semantic highlighting;
- documentation generation;
- a build and package model;
- cross-compilation support.

Editor support should be developed in layers:

1. a TextMate grammar for immediate highlighting of `.fl` files;
2. a Tree-sitter grammar for structural parsing and highlighting;
3. Language Server Protocol semantic tokens driven by the compiler's type information.

Important initial highlighting categories include function modifiers, control-flow keywords, primitive types, type names, function names, parameters, variables, properties, numeric literals, strings, comments, and the built-in constants `none`, `null`, `true`, and `false`.

## 18. Illustrative program

The following example demonstrates the current core direction. It is illustrative; not every construct shown has a finalized grammar or implementation.

```flecha
error CopyError {
    InvalidPointer,
    DestinationTooSmall,
}

unsafe (usize, CopyError) copy(
    []u8 destination,
    []const u8 source
) {
    if destination.ptr == none || source.ptr == none {
        return (0, CopyError.InvalidPointer);
    }

    if destination.len < source.len {
        return (0, CopyError.DestinationTooSmall);
    }

    memory_copy(destination.ptr, source.ptr, source.len);
    return (source.len, none);
}

i32 main() {
    u8[128] destination;
    []const u8 source = "Flecha";

    usize copied;
    CopyError err;

    unsafe {
        (copied, err) = copy(destination[:], source);
    }

    if err != none {
        return 1;
    }

    return 0;
}
```

## 19. Open design questions

The following decisions are intentionally not finalized:

- module, import, and visibility syntax;
- additional declaration forms, including whether to introduce `var`;
- string literal representation and encoding;
- slice syntax and array-to-slice conversion;
- struct, union, enum, and generic syntax;
- ownership, borrowing, and deterministic cleanup;
- allocator interfaces;
- arithmetic overflow behavior;
- floating-point equality involving NaN and signed zero;
- compile-time execution and metaprogramming;
- tuple layout and ABI rules;
- optional values distinct from primitive zero;
- error propagation syntax;
- generator and coroutine syntax using the reserved `yield` keyword;
- concurrency and memory-model rules;
- supported platforms and calling conventions;
- whether `null` remains a separate pointer spelling or is replaced entirely by `none`.

These questions should be resolved by prioritizing predictable behavior, a small implementation surface, useful diagnostics, and compatibility with low-level programming.

## 20. Initial implementation milestones

1. Recognize `.fl` files and produce source-located diagnostics.
2. Implement tokens, comments, literals, and primitive type names.
3. Parse functions, blocks, parameters, and `return` statements.
4. Type-check primitive expressions and local variables.
5. Implement calls and a minimal C backend.
6. Implement tuples, tuple returns, and destructuring.
7. Implement universal `none`, zero initialization, `==`, and `===`.
8. Error types and tuple-based error handling are implemented; payloads and propagation sugar remain future work.
9. Implement pointers, slices, and explicit unsafe blocks.
10. Introduce a compiler-owned intermediate representation.
11. Add an LLVM backend and native debug information.
12. Expand platform support, tooling, and the standard library.
13. Begin self-hosting once Flecha can express the compiler without special cases.

## 21. Guiding principle

Flecha should make low-level programming direct without making dangerous behavior invisible. Its defining principle is:

> Mechanically predictable code with locally visible costs.

New features should be evaluated against that principle before they are added to the language core.
