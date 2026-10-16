# Contributing to Flecha

Thank you for helping build Flecha. The project welcomes bug reports,
documentation improvements, design discussions, tests, and focused pull
requests. Flecha is still young, so clear reasoning and maintainable code are
more valuable than the size or speed of a contribution.

## Human authorship and AI-assisted contributions

Flecha is intended to become a language that people can understand, maintain,
and eventually use to implement more of its own toolchain. Human authorship is
therefore an explicit project value.

AI agents and generative coding tools may author implementation changes only
for core compiler and toolchain features written in C++. This includes the C++
front end, semantic analysis, lowering, LLVM code generation, native compiler,
and directly related C++ tests.

AI agents must not author:

- Flecha standard-library code, including files under `std/`;
- features implemented in Flecha source (`.fl` files);
- any future self-hosted compiler, toolchain, or runtime code;
- non-core product features, examples, or editor integrations; or
- a mixed contribution in which generated C++ is used as a route to generate
  or substantially design code in one of the areas above.

Those contributions must be written by a human. Using ordinary, deterministic
tools such as compilers, formatters, linters, and search-and-replace is not
considered AI authorship.

When an AI agent was used in an allowed C++ contribution, disclose it clearly
in the pull-request description. State which parts were agent-authored or
agent-assisted. The human contributor must understand and take responsibility
for every submitted line, verify its provenance and licensing, and be able to
explain or revise it during review. Prefer separating human-authored Flecha
work and AI-assisted C++ work into different pull requests.

Maintainers may ask for a contribution to be rewritten or may close it when
its authorship is unclear or this policy is not followed. When in doubt, open
an issue before writing code.

## Before you begin

For a bug fix, search the issue tracker and include a minimal reproduction.
For a language or library change, start with a design discussion. Describe:

- the problem and intended behavior;
- syntax and type-system effects;
- diagnostics and failure behavior;
- lowering, code-generation, and runtime implications;
- compatibility or migration concerns; and
- alternatives considered.

The documents in `docs/` and `SPEC.md` include both implemented behavior and
proposals. Compiler tests and diagnostics are the authority when the two do
not yet agree.

## Building and testing

The main requirements are CMake 3.20 or newer, Ninja (or another CMake
generator), a C++23 compiler, and the LLVM development libraries and tools.
Build the compiler and Longbow with:

```sh
make build
```

Run the complete test suite with:

```sh
make test
```

Useful focused checks include:

```sh
make test-semantic
make test-neovim
make test-sanitizers
```

Release-related changes should also produce and inspect the distributable
archives:

```sh
cmake --build build --target package
```

See `README.md` for setup details and additional native smoke-test targets.

## Pull-request expectations

Keep pull requests focused and make their intent easy to review. A good pull
request:

- explains the problem, the chosen approach, and important tradeoffs;
- links the relevant issue or design discussion;
- includes regression tests for changed behavior;
- preserves source ranges and stable diagnostic identifiers where practical;
- updates `SPEC.md`, design documents, and examples when behavior changes;
- follows `CODE_STYLE.md` for C++ changes;
- avoids committing build outputs, credentials, or unrelated formatting; and
- reports the commands used to validate the change.

Changes to the language should normally cover the full path through parsing,
semantic analysis, lowering, code generation or runtime behavior, diagnostics,
and tests. If a pull request intentionally implements only part of that path,
state the limitation explicitly.

All continuous-integration checks must pass. Platform-specific changes should
be tested on every affected operating system when possible; the repository CI
builds and exercises Linux, Windows, and macOS packages.

## Reporting bugs

Please include:

- the Flecha commit or release version;
- operating system, architecture, C++ compiler, and LLVM version;
- the smallest source file or command that reproduces the problem;
- expected and actual behavior; and
- complete diagnostics or a backtrace, if available.

Remove secrets and private paths from logs before posting them.

## Review and conduct

Reviews may ask for simpler code, stronger tests, a clearer design, or a
smaller change. Treat review as collaboration: discuss the reasoning, update
the change when appropriate, and keep follow-up commits understandable.

Be respectful and patient with contributors of every experience level. Focus
technical discussion on the work, assume good intent, and make room for people
to learn without lowering the project's correctness standards.

By contributing, you agree that your work may be distributed under the terms
of the repository's `LICENSE`.
