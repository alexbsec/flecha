# Raw-pointer ownership contracts

`own` and `lend` describe cleanup responsibility at function boundaries and in
structure fields. They do not change the raw-pointer representation or ABI.
`const` still controls mutation. Plain raw-pointer types remain ownership-neutral.
References use their existing checked-access rules and never take either keyword:
`own &T` and `lend &T` are invalid.

```flecha
unsafe void release(own *i32 pointer) { free(pointer); }
unsafe i32 read(lend *const i32 pointer) {
    if pointer == null { return 0; }
    return *pointer;
}
unsafe (own *i32, error) create() { return new<i32>(42); }
unsafe own *i32 forward(own *i32 pointer) { return pointer; }
```

## Parameters and returns

A `lend` parameter permits access during the call. It may be forwarded to another
lender, but cannot be freed, consumed, returned, or retained in escaping storage.
Local aliases retain the same restrictions. Lending a raw pointer does not create
a checked reference or impose reference-style exclusivity.

An `own` parameter transfers responsibility unconditionally at the call boundary.
Every normal callee exit must free the allocation, forward it to another consumer,
store it in an owning field, or return it through an owning result. An error does
not restore the caller's ownership: APIs that give it back must return an owning
result explicitly. This also applies to recursive consuming calls.

Transfer makes existing tracked aliases unavailable for access or cleanup. It
does not null their runtime storage. Passing the same allocation simultaneously
as an owner and another argument is rejected. A pending deferred use or cleanup
prevents conflicting transfer. Deferred calls retain the existing capture and
execution timing: `defer release(pointer)` schedules consumption for scope exit.

An owning return transfers cleanup responsibility to its caller. Ignoring a live
owning result is an error. Tuple results can annotate individual pointer elements,
including nested tuple results. Owning or lending tuple parameters, nested
pointer contracts, local contract declarations, and lending returns are not
supported. In an annotated function, returning tracked ownership through an
unannotated pointer result is rejected.

Null carries no cleanup obligation. Passing or returning null as an owner is valid;
ownership does not guarantee non-nullness. Visible factory return paths preserve
allocation-error/null relationships, including `new<T>` failure paths. An arbitrary
error result does not imply that an accompanying owning pointer is null.

## Owning fields and tear

```flecha
structure Box { own *i32 value; };
unsafe Box Box::init(own *i32 value) override {
    return build Box { self.value = value; };
}
unsafe void Box::tear(&self) override { free(self->value); }
```

Owning fields require an explicit `tear`. The compiler independently checks that
its normal exits discharge every live owning field. `tear` explicitly frees or
transfers the stored allocation; destroying the raw-pointer field performs no
additional free. Null fields need no cleanup.

Existing lifecycle rules remain: `tear` is compiler-invoked and cannot be called
explicitly. A custom tear makes its structure noncopyable, and an unsafe tear also
makes it immovable. Transferring a pointer to an immovable structure does not move
the structure. Fresh construction still goes directly into its final storage.

The initial field implementation supports initialization in `build` and access
through the structure's `self` methods. Omitted owning fields start null, as other
raw-pointer fields do. Methods other than `tear` receive only lent access to their
owning fields. Taking an address of an owning field, replacing a live field,
extracting ownership outside `tear`, declaration initializers, and direct access
through other receivers are rejected. These restrictions keep field ownership
encapsulated until field replacement and extraction have explicit state tracking.

## Checking boundaries

Annotated functions are verified independently, even when unused. Calls use their
contracts, so annotated recursion does not require recursive body interpretation.
Known-null calls are checked against the body as well. Unannotated helpers retain
the existing bounded visible-body analysis. Tracked raw pointers passed to an
annotated function need explicit parameter contracts; indirect mutation through
an address of a tracked pointer slot is not yet summarized.

The existing allocation-analysis path budgets remain, with an additional limit of
128 nullable call configurations per compilation unit. Repeating an owning return
site with its previous result still live is rejected. Unsupported ownership effects
fail before object emission rather than silently losing tracking.

This does not introduce external-function support, safe heap references, general
moves, or complete memory-safety verification. Unverified external raw-pointer
validity, aliasing, nullability, and stack lifetimes remain unsafe obligations.
Dynamic arrays remain the next storage foundation; these contracts do not yet
provide variable-sized allocation, initialized-length tracking, or growth rules.
