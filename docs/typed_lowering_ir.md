# Typed lowering IR

The lowering layer is the internal boundary between semantic analysis and code
generation. It does not define or change Flecha source syntax.

`Lowerer` accepts a syntax tree, its `SemanticModel`, and the diagnostic bag
produced by the frontend. Programs with frontend errors are rejected. Valid
programs become a move-only `BoundProgram` where every expression has a
resolved semantic type, every name refers directly to its symbol, and every
node retains a half-open source range.

The bound program contains functions, blocks, local declarations, expression
statements, returns, conditionals, literals, variable and function references,
unary and binary operations, assignments, and direct calls. It holds
non-owning references to symbols owned by the semantic model, so the syntax
tree and semantic model must outlive the bound program.

Conditional syntax is normalized for backend consumption. `else if` becomes
an `if` nested in the preceding alternative block. An `also` clause becomes a
conditional appended to the successful body of its owning branch. This
preserves the language rule that `also` is evaluated only after its owner ran.

`DumpBoundProgram` produces deterministic, human-readable text intended for
debugging and tests. It is not Flecha syntax and is not a serialization format.

`BoundDeferStatement` retains a resolved direct call separately from immediate
expression statements. Its arguments capture at registration, and codegen
invokes the call during scope cleanup. See [defer.md](defer.md).
