# Flecha 0.1.0 release milestone

Flecha 0.1.0 is the first installable experimental release. Self-hosting is
not part of this milestone. The release is complete only when the same source
revision produces usable native compiler packages for Linux, Windows, and
macOS.

## What ships

Each platform archive contains:

- the [`flecha` compiler](compiler.md);
- the [`longbow` project tool](longbow.md);
- the version-matched native runtime archive;
- the Flecha standard-library source packages;
- Visual Studio Code syntax highlighting and language configuration;
- Neovim filetype, indentation, and syntax support;
- the license and introductory documentation.

The editor bundles are grammar-based. A language server, completion,
go-to-definition, and semantic highlighting are post-0.1 work.

## Supported platforms

The initial support contract covers current 64-bit releases of:

- Linux on x86-64;
- Windows on x86-64;
- macOS on Apple silicon and x86-64 where the build service still provides an
  x86-64 runner.

Every platform must be able to run `flecha check`, emit an object, link and run
a native Flecha program, and complete the Longbow `new`, `check`, `build`, and
`run` workflow. A frontend-only build does not satisfy the release gate.

## Compiler gates

- `flecha --version` reports `flecha 0.1.0`.
- A clean checkout configures and builds with a documented C++23 compiler and
  LLVM version on every supported platform.
- The portable unit suite and native end-to-end suite pass on every platform.
- Object names, executable suffixes, runtime discovery, process execution, and
  native linking behave according to the host platform.
- An installed compiler still builds programs after its installation tree is
  moved to another directory.

## Longbow gates

- `longbow --version` reports `longbow 0.1.0`.
- `longbow new`, `init`, `check`, `build`, `run`, and `clean` pass on every
  supported platform.
- A fresh user can unpack a release, create a project with `longbow new`, and
  run it without referring to the Flecha source checkout.
- Local dependencies and native link inputs work with native path syntax.

## Distribution and editor gates

- Release archives contain the compiler, Longbow, runtime, standard library,
  editor bundles, license, and documentation.
- The Visual Studio Code manifest and TextMate grammar are validated in tests.
- The Neovim syntax test passes, and its installed directory can be added to
  `runtimepath` without the source checkout.
- Every published archive has a checksum and concise changelog.

## Automation

Pull requests and pushes to `master` run `.github/workflows/ci.yml`. The
workflow checks patch whitespace, runs the full Linux test and editor suites,
runs AddressSanitizer and UBSan, and performs native build, Longbow, package,
and installed-layout smoke tests on Linux, Windows, Intel macOS, and Apple
silicon macOS.

Publishing a GitHub Release runs `.github/workflows/release.yml`. Use a semantic
version tag matching the compiler and VS Code extension, for example `v0.1.0`.
The tag must point to a commit contained in `master`. All platform archives are
built and retained as workflow artifacts first. Only after every platform
succeeds does the final job attach the archives and `SHA256SUMS` to the GitHub
Release.

```sh
git tag v0.1.0
git push origin v0.1.0
```

Create a GitHub Release for that tag and publish it. Draft releases do not start
the workflow until they are published. Re-running a successful release workflow
replaces assets with the same names.

## Work remaining

The repository now has Linux, Windows, Intel macOS, and Apple-silicon macOS
package jobs. Each job builds the tools, runs native compiler and Longbow smoke
tests from both the build and installed layouts, and creates archives. The
remaining work is:

1. Make the platform package jobs green on the hosted runners and pin the
   exact supported LLVM/toolchain versions from those results.
2. Validate native linker discovery with the documented toolchain on every
   platform and improve diagnostics for missing host SDK components.
3. Make the remaining native test harnesses use platform object and executable
   suffixes.
4. Replace or conditionally adapt Unix-only test fixtures that use `fork`,
   `<unistd.h>`, GNU linker wrapping, and symlink assumptions.
5. Decide whether release archives bundle LLVM shared libraries or require a
   separately installed, version-matched LLVM distribution.
6. Reconcile the specification and backend documentation with the exact
   v0.1.0 feature set and limitations.
7. Package the VS Code directory as a `.vsix` and document one-command editor
   installation on every platform.

Package registries, remote dependencies, an editor language server, a
formatter, cross-compilation, debug information, and self-hosting remain
post-0.1 work.
