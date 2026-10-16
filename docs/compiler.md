# Flecha compiler

`flecha` is the language compiler shipped in the Flecha 0.1.0 distribution. It
loads a `.fl` entry module and its imports, performs semantic and ownership
checks, lowers the program, and can emit LLVM IR, a native object, or a native
executable for the host platform.

## Commands

```text
flecha --version
flecha check INPUT [--root DIR] [--lib NAME=DIR ...]
flecha build INPUT -o OUTPUT [--root DIR] [--lib NAME=DIR ...]
             [--link OBJECT-OR-LIBRARY ...]
flecha --emit-llvm INPUT [-o OUTPUT] [--root DIR] [--lib NAME=DIR ...]
flecha --emit-object INPUT -o OUTPUT [--root DIR] [--lib NAME=DIR ...]
```

`check` runs the complete frontend without creating an artifact. `build` emits
and links a host-native executable with the Flecha runtime. `--emit-object`
stops before linking. `--emit-llvm` writes textual LLVM IR, or prints it when
`-o` is omitted.

`--root` supplies the source root used by `root::` imports. Each `--lib`
option maps one `lib::` namespace to a source directory. `--link` adds a native
object or library when building an executable containing `extern`
declarations.

The compiler invokes a host C++ linker driver for executable builds. Flecha
0.1.0 release packages must validate this path on Linux, Windows, and macOS.

## Installed layout

The compiler locates its version-matched runtime archive relative to the
installed executable. `FLECHA_RUNTIME_LIBRARY` can override that path for
development and diagnostics. Moving a complete Flecha installation or
unpacked release archive must preserve the relative `bin` and `lib` layout.

Most users should use [Longbow](longbow.md), which supplies the project root,
dependencies, output paths, and native link inputs from `longbow.toml`.
