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
