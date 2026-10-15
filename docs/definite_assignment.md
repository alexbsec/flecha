# Definite assignment

Typed value declarations may omit their initializer:

```flecha
i32 answer;
string message;

answer = 42;
message = "ready";
```

The compiler reports `FLECHA-S0021` when a variable may be read before every
control-flow path has assigned it. Assignments made only inside a loop are not
definite after that loop because the loop may execute zero times.

This syntax currently supports all integer and floating-point types, `bool`,
`char`, and owning `string`. `void` is never a variable type. Borrowed
`stringv` declarations require an initializer and otherwise produce
`FLECHA-S0022`; delayed borrow initialization will be considered when Flecha's
lifetime analysis can carry borrow origins through control-flow joins.

Inferred declarations continue to require an initializer:

```flecha
answer := 42;
```

## Reachable control flow

Analysis runs after name binding and uses resolved symbols, so shadowed names
remain distinct. Each function gets a control-flow graph with reads, writes,
declarations, conditions, and explicit transfer edges. A worklist computes a
fixed point: at each join, a variable is initialized only if it is initialized
on **every reachable predecessor**. Parameters start initialized. A declaration
starts a fresh initialization state each time execution reaches it.

A terminating branch contributes no fallthrough edge. For example:

```flecha
i32 choose(bool condition) {
    i32 value;
    if condition {
        return 0;
    } else {
        value = 42;
    }
    return value;
}
```

The same rule applies to `break`, `proceed`, and `jumpto`. Unreachable reads do
not produce `FLECHA-S0021`; name resolution and type checking still apply to
unreachable source. Diagnostics identify the exact read expression and are
reported once, after convergence, in source order within each function.

## Loops and conditional also clauses

`proceed` targets the innermost loop's condition, skipping its remaining body
and `also` clause. `break` targets that loop's exit. Normal body completion
reaches the conditional `also` clause, if present, before the next iteration.
Nested loops maintain separate targets. Assignments on a backedge cannot
initialize a read on the first iteration.

The condition's false edge permits zero iterations, so assignments made only
inside a loop cannot normally initialize a variable after it. Assignments in
a condition itself are modeled in evaluation order. Boolean `and` and `or`
short-circuit: their right operands run only on the corresponding condition
edge. Integer bitwise operations evaluate both operands.

Conditions conservatively retain both outcomes, even for literals. This pass
does not perform constant folding or correlate repeated tests of a value.

## Positions, jumps, and scopes

Forward and backward `jumpto` edges connect directly to their resolved
`#position`, including positions in other nested loops. Positions join both
fallthrough and all reachable jump predecessors. Skipping a declaration or
assignment never initializes its variable:

```flecha
void rejected(bool run) {
    while run {
        jumpto use;
        i32 value = 42;
        #use
        stdoutln(value); // FLECHA-S0021 at value
        break;
    }
}
```

Leaving a scope removes its locals from initialization state. Jumping back
into that scope cannot reuse state from an earlier activation. A jump into a
scope may be accepted when all subsequent reads are preceded by assignments;
normal lexical visibility still governs which symbols can be named.

Owning strings have null storage at function entry, making cleanup safe even
when a jump skips their declarations. Cleanup emission includes all slots in
the scopes being exited, including later declarations and temporaries that
may have executed before a backward jump to an earlier exit. Transfers of
ownership clear the source storage so repeated execution cannot double-free
an allocation. This does not change the requirement that `stringv`
declarations have initializers or extend borrow-origin analysis.

## Verification

`make test` includes semantic, lowering, LLVM verification, and native
control-flow regressions. `make test-sanitizers` also runs the native
control-flow fixture with an instrumented runtime and leak detection enabled.
`make native-definite-assignment` preserves the original example's output and
exit status of 42.
