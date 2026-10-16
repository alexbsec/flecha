# Flecha standard-library development guidelines

These guidelines apply to Flecha source in `std/`. Follow the naming conventions
in [library API style](library_api_style.md) as well.

## Expand nonempty brace-delimited bodies

Never put a nonempty body on one line, even when it contains only one statement.
Put the opening brace on the declaration or control-flow line and the closing
brace on its own line. An empty body may use `{}` on the same line.

```flecha
i32 test() {
    return 1;
}

void empty() {}
```

This applies to functions, structures, conditionals, loops, unsafe blocks,
`build` expressions, error declarations, and every other brace-delimited body.
For example, `return build RawArray<T> {};` is allowed because its body is empty.

Structure declarations retain their required trailing semicolon:

```flecha
structure Counter {
    i32 value;
};
```

## Separate declarations

Leave at least one blank line and at most two blank lines between declarations.
This includes structure fields. Do not place multiple declarations or statements
on the same line.

```flecha
structure Position {
    i32 x;

    i32 y;
};

i32 first() {
    return 1;
}

i32 second() {
    return 2;
}
```

## Group statements by purpose

Keep statements that perform one related task together. Use a blank line when
the code moves to a different task. Do not compress unrelated operations into a
single uninterrupted block or separate every statement mechanically.

Keep an operation and its immediate error check together. Keep creation and
configuration of the same value together. Separate a completed conditional from
the next distinct operation.

The following layout illustrates those groups; the request functions and types
are placeholders:

```flecha
if ready {
    prepare();
}

y := x * factor;

req := request();
req.status = 300;

resp, err := response();
if err != none {
    return err;
}
```
