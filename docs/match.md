# Enum matching

`match` is an exhaustive expression for nominal scalar enums. It evaluates its
input exactly once, selects one arm, and evaluates only that arm.

```flecha
enum TokenKind: u16 {
    Identifier,
    Number,
    End,
}

bool TokenKind::ends_input(&const self) {
    return match *self {
        TokenKind.End => true,
        _ => false,
    };
}
```

## Patterns and arms

An arm pattern is either a qualified enum variant such as
`TokenKind.Identifier` or the wildcard `_`. Flecha retains its existing `.`
spelling for enum variants while using Rust-style `match`, `=>`, comma-separated
arms, and an optional trailing comma.

Without a wildcard, every variant of the matched enum must appear exactly once:

```flecha
i32 precedence(TokenKind kind) {
    return match kind {
        TokenKind.Identifier => 1,
        TokenKind.Number => 2,
        TokenKind.End => 0,
    };
}
```

A wildcard covers every variant not named by an earlier arm and must be the
last arm. Duplicate variants, unknown variants, patterns belonging to another
enum, and arms after a wildcard are compile-time errors.

## Result types

Every arm contains an expression. Its type must be assignable to the match
expression's result type. An enclosing declaration, argument, assignment, or
return supplies contextual type information; otherwise the first arm determines
the result type.

The initial implementation matches enum values only. Destructuring patterns,
guards, ranges, integer and string patterns, and block-bodied arms are not yet
supported. Match expressions also cannot currently produce borrowed-reference
results. These restrictions do not affect ordinary scalar, enum, `stringv`, or
owned-value results.

## Lowering

The compiler lowers a match to a switch over the enum's backing integer. The
input is evaluated once. Exhaustiveness makes every valid enum value select an
arm; the backend marks any impossible representation as unreachable when no
wildcard is present.
