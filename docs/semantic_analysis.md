# Semantic analysis

## Pipeline

Flecha processes source in three stages: the tokenizer produces ranged tokens,
the parser builds the move-only syntax tree, and `SemanticAnalyzer` binds and
checks that tree. Syntax types (`core::TypeNode` and `core::PrimitiveType`) stay
in the AST. `semantic::SemanticType` is the resolved type used by symbols and
expression checking.

Analysis first registers every top-level function so calls may refer to
functions declared later in the file. It then analyzes each function body in
source order. The resulting `SemanticModel` provides:

- the symbol declared by a function, parameter, or local declaration;
- the symbol resolved for each `NameExpression`;
- the resolved type of each expression;
- the compilation-unit, function, and block scopes created during analysis.

## Ownership and lifetimes

The AST exclusively owns its children with `std::unique_ptr`. It remains alive
while its semantic model is queried. A `SemanticModel` exclusively owns all
symbols and scopes. Scope tables and AST binding tables contain non-owning
pointers to those model-owned objects. Symbols and scopes are individually
heap-allocated, so moving a model does not invalidate those pointers. The
model never owns AST nodes and must not outlive the AST when its AST-keyed
queries are used.

## Scopes and declarations

The compilation-unit scope contains functions. Each function has a child scope
containing its parameters and top-level body locals. Conditional bodies,
`else` bodies, and `also` bodies introduce nested lexical scopes. Lookup starts
in the current scope and walks outward. A declaration name may appear only once
in a scope; a nested declaration may intentionally shadow an outer symbol.

Function declarations are callable and not writable. Parameters and local
variables are writable under the current language rules. A `:=` declaration
always creates a local in the current scope; it never updates an outer symbol.
Both explicit and inferred declarations are registered after their initializer
is analyzed, so a declaration cannot refer to itself and use-before-declaration
is diagnosed.

## Types and inference

Semantic types cover every syntax primitive plus the literal-only `null` and
`none` types, the internal `function` type, and an error-recovery type. Literal
defaults are:

| Literal | Inferred type |
| --- | --- |
| Integer | `i32` |
| Floating point | `f64` |
| `true`, `false` | `bool` |
| Character | `char` |
| String | `stringv` |
| `null` | `null` |
| `none` | `none` |

Parentheses and valid unary, binary, assignment, name, and call expressions
propagate their resolved types. Distinct numeric types are not implicitly
coerced. `none` is contextually assignable to non-void value types, matching
the language specification's universal-zero rule. Arithmetic requires equal
numeric operand types; comparisons produce `bool`; logical/bitwise operators
require equal boolean or integer types. Pointer dereference is parsed but is
not yet semantically supported because pointer syntax types are not in the
current AST.

Calls currently require a directly resolved function symbol. Analysis checks
callability, argument count, and each known argument type. Function values and
overloads are not yet part of the language model.

## Diagnostics and recovery

Semantic diagnostics use stable codes `FLECHA-S0001` through
`FLECHA-S0013`. Declaration-name diagnostics use the exact identifier token
range retained by the AST; expression diagnostics use exact half-open AST
ranges. The analyzer returns an error type after a failed resolution or type
check so dependent checks suppress redundant cascades while independent
statements continue to produce useful diagnostics.

| Code | Meaning |
| --- | --- |
| `FLECHA-S0001` | Duplicate declaration in one scope |
| `FLECHA-S0002` | Unknown name |
| `FLECHA-S0003` | Incompatible declaration or assignment type |
| `FLECHA-S0004` | Invalid semantic assignment target |
| `FLECHA-S0005` | Resolved symbol is not writable |
| `FLECHA-S0006` | Callee is not callable |
| `FLECHA-S0007` | Wrong argument count |
| `FLECHA-S0008` | Incompatible call argument |
| `FLECHA-S0009` | Invalid unary or binary operand types |
| `FLECHA-S0010` | Non-boolean condition |
| `FLECHA-S0011` | Invalid return type |
| `FLECHA-S0012` | Inferred declaration has no usable value type |
| `FLECHA-S0013` | Invalid variable type, currently `void` |

Parser diagnostics remain separate. In particular, `FLECHA-P0003` preserves
the parser's invalid-assignment-target report, and semantic analysis adds
`FLECHA-S0004` when the recovered AST is analyzed.
