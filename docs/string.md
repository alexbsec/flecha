# Strings

Flecha has two complementary text types:

- `stringv` is an immutable, non-owning view of bytes.
- `string` is an immutable value that owns its backing allocation.

The initial string model is byte-oriented. Lengths and indices count bytes,
and `char` is a `u8`. Unicode decoding and code-point iteration belong in a
later text library and do not change the storage contract described here.

## Literals and ownership

A string literal has type `stringv` and refers to static immutable storage:

```flecha
view := "flecha";              // inferred stringv
string owned = "flecha";       // creates an owning copy
```

An owning string stores a data pointer, byte length, and capacity. Its runtime
storage is null-terminated for interoperability, but the explicit byte length
is authoritative and does not include the terminator. Embedded null bytes are
valid.

## View-to-string conversion

Converting a view to `string` allocates storage and copies its bytes. Both the
general cast syntax and an idiomatic member are supported:

```flecha
stringv view = "flecha";

string explicitOwned = view.cast<string>;
string idiomaticOwned = view.string();
```

`view.string()` is preferred in normal Flecha source because it makes the
creation of an owning value conspicuous. `cast<string>` remains available for
generic conversion code and preserves the language's chainable cast model.
The two forms have identical semantics and diagnostics.

Converting `string` to `stringv` is a non-allocating borrow:

```flecha
string owned = "flecha";

stringv explicitView = owned.cast<stringv>;
stringv idiomaticView = owned.view();
```

`owned.view()` is the idiomatic spelling. `cast<stringv>` exists for symmetry
with `stringv.cast<string>` and for generic conversion code. Both forms create
the same immutable borrow and do not allocate or copy bytes.

The view must not outlive or observe destruction or replacement of its owner.
Until full lifetime analysis exists, the compiler will conservatively reject
returning or otherwise escaping a view derived from a local owning string.

## Initial operations

Both string types provide byte-based inspection:

```flecha
text.len();              // u32
text.at(0);              // checked char
text[0];                 // checked char, equivalent to at(0)
text.substr(0, 3);       // allocation-free stringv
```

Following Flecha library API style, member names use `camelCase`. Existing
single-word members such as `len`, `at`, and `substr` already follow that rule;
multiword members use spellings such as `startsWith` and `isEmpty`.

Concatenation creates a new owning value:

```flecha
string message = "hello, " + name;
```

It accepts `string` and `stringv` operands. Neither operand is modified.

## Value and cleanup semantics

The first implementation uses predictable value semantics:

- Copy initialization and assignment produce an independent owned value.
- Assignment destroys the destination's previous allocation after the new
  value has been created successfully.
- A `string` parameter receives an owned value.
- Returning a `string` transfers the result to the caller.
- `stringv` never destroys storage.
- Every owned local is destroyed exactly once on normal exit, `return`,
  `break`, `proceed`, and `jumpto` edges that leave its lifetime.

Move analysis may later remove unnecessary copies without changing observable
program behavior.

## Runtime responsibilities

The runtime will provide versioned C ABI operations for:

- constructing an owned string from `stringv`;
- cloning an owned string;
- exposing an owned string as a non-owning `stringv` in generated code;
- concatenating string values;
- destroying owned storage;
- reporting allocation failure without allowing C++ exceptions across the
  ABI boundary.

Bounds checking for `at`, indexing, and `substr` follows the existing
`stringv` policy: an invalid dynamic range reports a runtime error and
terminates safely.
