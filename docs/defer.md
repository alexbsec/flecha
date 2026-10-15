# Deferred calls

`defer function(arguments);` captures a direct function call now and executes it
when the current lexical scope exits. See [the runnable example](../examples/defer.fl).

```flecha
void report(i32 value) { stdoutln(value); }
i32 main() {
    i32 value = 10;
    defer report(value);  // captures 10
    value = 20;
    {
        defer report(1);
        defer report(2);
    }                    // prints 2, then 1
    return 42;           // prints 10 before returning 42
}
```

## Capture and execution

- Arguments evaluate once, left to right, when execution reaches the statement.
  Their side effects happen immediately; the outer call runs later.
- Scalar, raw-pointer, and fixed-array arguments capture their current values.
  Reassigning the original variable does not change the captured value.
- Both `string` and `stringv` arguments get independent owned byte snapshots.
  This copy can allocate. A `stringv` parameter receives a view of its snapshot
  during the deferred call. All snapshots are destroyed after that call.
- References and slices capture borrowed handles, without copying their targets.
  Their loans remain active until scope exit, including the lifetime of any
  parent reborrow. They must not outlive their owners.
- Each scope executes its registrations in reverse registration order. Inner
  scopes finish before outer scopes. Calls run before that scope destroys its
  owning strings and compiler string temporaries.
- Registration happens only if the statement executes. The same statement can
  register multiple calls, each with independent captures.
- Non-void results are discarded. Owning string results are destroyed.

Only statically resolved direct function calls are supported, including
`stdout` and `stdoutln`. Parenthesizing the call is allowed. Deferred blocks,
member calls, closures, and indirect function calls are not supported in this
milestone. `defer` does not require unsafe permission by itself; an unsafe callee
or argument operation still requires an enclosing unsafe block or function at
the registration site.

## Scope exits and control flow

Calls run on ordinary fallthrough, `return`, `break`, `proceed`, and `jumpto`
edges that leave their scope. A return expression evaluates before cleanup, so
cleanup cannot change the already computed return value. A counted loop's body
cleans up before its update executes, including on `proceed`.

A jump within the same scope keeps all pending registrations. A backward jump
can register another instance; none are overwritten. Jumps entering a scope
initialize its cleanup state even when they skip its opening brace. Skipped
`defer` statements do not register. Existing rules still restrict positions and
jumps to loops.

An `also` clause follows existing lexical-scope rules: the owning body remains
active through its `also` clauses, and a clause has its own nested scope.

Traps and immediate process termination do not unwind scopes. If argument
evaluation traps, that call is not registered. If a deferred callee traps,
remaining calls do not execute. Flecha does not introduce exception handling.

## Checked borrows

```flecha
void set(&i32 target) { *target = 42; }
i32 main() {
    i32 value = 0;
    { defer set(&value); }
    return value;        // 42; the deferred borrow has ended
}
```

An exclusive deferred borrow prevents conflicting reads, writes, moves, and
rebindings until its scope exits. A shared deferred borrow permits reads but
prevents mutation. This initial analysis conservatively rejects two pending
exclusive borrows of the same owner, even though their calls would execute
sequentially. Deferred calls do not count as having already initialized or
modified their targets.

Raw pointers retain their existing unsafe contract. `defer` alone does not
prove pointer validity, prevent double frees, or supply ownership tracking.
The system allocator and resource-consumption analysis remain future work.

## Implementation and validation

Each active scope has a linked stack of captured call records, dispatched by
call site. Registrations allocate records on the function stack, and scope exit
restores the stack after draining them. Repeated registration within one active
scope uses space proportional to its pending calls; unbounded registration can
exhaust the stack. The queue itself requires no heap allocator.

The shared CFG records argument evaluation at registration. Synthetic capture
symbols retain borrowed origins until the matching scope-exit edge. LLVM cleanup
blocks handle all exits, including jumps into sibling scopes.

Unit tests cover parsing, diagnostics, borrow lifetimes, lowering, and verified
LLVM IR. Native tests cover capture order, strings/views, arrays, deferred return
values, all exit forms, traps, backward jumps, and repeated scope entry with
large captures. The sanitizer target includes native defer tests and leak checks
for the runtime's string allocations; generated Flecha object code itself is
not sanitizer-instrumented.
