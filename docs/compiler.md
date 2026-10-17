# Flecha compiler

`flecha` is the language compiler shipped in Flecha release archives. It
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

The compiler invokes a host C++ linker driver for executable builds. Release
packages validate this path on Linux, Windows, and macOS.
Set `FLECHA_LINKER` to a driver name or full path to override automatic
discovery. A configured override is authoritative and does not fall back to
another driver.

### Binary release requirements

Release archives include the matching LLVM shared runtime, so using an archive
does not require LLVM development files. Emitting and linking an executable
does require a host C++ linker driver and the platform's native SDK components:

- Linux requires `clang++`, `g++`, or `c++`, together with the system C/C++
  development and startup files. The release smoke test installs `binutils`,
  `clang-18`, and `libc6-dev` on Ubuntu 24.04.
- Windows packages target MSYS2 UCRT64 and require its `clang++` driver on
  `PATH`. The release smoke test installs `mingw-w64-ucrt-x86_64-clang`; it does
  not install the LLVM development package.
- macOS requires the Xcode command-line tools, including `clang++` and the
  macOS SDK. Install them with `xcode-select --install` when they are absent.

`flecha check` and `flecha --emit-object` do not invoke the system linker.
Building Flecha itself from source still requires the LLVM development package
listed in the repository build requirements. The exact supported ranges,
platform packages, and release baselines are maintained in
[`toolchains.md`](toolchains.md).

### Source-build LLVM requirement

Flecha supports LLVM 17 through 23. CMake reports the detected version and
rejects missing, incomplete, older, or newer LLVM installations before
compilation. When LLVM is installed outside the default search path, configure
with its CMake package directory:

```sh
cmake -S . -B build -G Ninja \
    -DLLVM_DIR="$(llvm-config --cmakedir)"
```

On macOS with Homebrew, use the `lib/cmake/llvm` directory below the selected
`llvm` formula prefix. On Windows, use the corresponding directory in the
MSYS2 UCRT64 LLVM installation.

## Installed layout

The compiler locates its version-matched runtime archive relative to the
installed executable. `FLECHA_RUNTIME_LIBRARY` can override that path for
development and diagnostics. Moving a complete Flecha installation or
unpacked release archive must preserve the relative `bin` and `lib` layout.

Most users should use [Longbow](longbow.md), which supplies the project root,
dependencies, output paths, and native link inputs from `longbow.toml`.
