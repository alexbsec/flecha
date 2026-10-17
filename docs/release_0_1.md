# Flecha 0.1 release contract

Flecha 0.1.0 is the first installable experimental release. Self-hosting is
not part of this milestone. The release is complete only when the same source
revision produces usable native compiler packages for Linux, Windows, and
macOS.

This page records the distribution contract shared by the 0.1 release line.
The [0.1.1 stabilization milestone](release_0_1_1.md) adds clean-install
artifact testing, pinned toolchains, improved linker diagnostics, and an
installable Visual Studio Code extension. Updating documentation on `master`
does not itself change the version reported by the tools or publish a release.

## What ships

Each platform archive contains:

- the [`flecha` compiler](compiler.md);
- the [`longbow` project tool](longbow.md);
- the version-matched LLVM shared runtime and its non-system dependencies;
- the version-matched native runtime archive;
- the Flecha standard-library source packages;
- Visual Studio Code syntax highlighting and language configuration;
- Neovim filetype, indentation, and syntax support;
- the license and introductory documentation.

The editor bundles are grammar-based. A language server, completion,
go-to-definition, and semantic highlighting are post-0.1 work.

## Supported platforms

The initial support contract covers:

- Linux on x86-64;
- Windows on x86-64;
- macOS on Apple silicon and x86-64 where the build service still provides an
  x86-64 runner.

The exact compiler, LLVM, linker, SDK, operating-system, and hosted-runner
baselines are maintained in [the toolchain matrix](toolchains.md). “Current” in
this historical contract does not extend support beyond that tested matrix.

Every platform must be able to run `flecha check`, emit an object, link and run
a native Flecha program, and complete the Longbow `new`, `check`, `build`, and
`run` workflow. A frontend-only build does not satisfy the release gate.

## Compiler gates

- `flecha --version` reports the package version.
- A clean checkout configures and builds with a documented C++23 compiler and
  LLVM version on every supported platform.
- The portable unit suite and native end-to-end suite pass on every platform.
- Object names, executable suffixes, runtime discovery, process execution, and
  native linking behave according to the host platform.
- An installed compiler still builds programs after its installation tree is
  moved to another directory.

## Longbow gates

- `longbow --version` reports the same package version as `flecha`.
- `longbow new`, `init`, `check`, `build`, `run`, and `clean` pass on every
  supported platform.
- A fresh user can unpack a release, create a project with `longbow new`, and
  run it without referring to the Flecha source checkout.
- Local dependencies and native link inputs work with native path syntax.

## Distribution and editor gates

- Release archives contain the compiler, Longbow, their shared runtime
  dependencies, the Flecha runtime, standard library, editor bundles, license,
  and documentation.
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
version tag matching the compiler and VS Code extension.
The tag must point to a commit contained in `master`. All platform archives are
built and retained as workflow artifacts first. Only after every platform
succeeds do fresh downstream jobs download each archive, extract it on a clean
runner, move the installation, and verify compiler, native-linking, Longbow, and
bundled-dependency behavior. Those jobs install only the documented host
linker/SDK prerequisites, not LLVM development packages. The final job then
attaches the archives, the installable `.vsix`, and `SHA256SUMS` to the GitHub
Release.

```sh
git tag vX.Y.Z
git push origin vX.Y.Z
```

Create a GitHub Release for that tag and publish it. Draft releases do not start
the workflow until they are published. Re-running a successful release workflow
replaces assets with the same names.

## Stabilization status and exclusions

The 0.1.1 stabilization work closes the original distribution gaps: platform
jobs use pinned and verified toolchains; linker discovery reports attempted
drivers and missing prerequisites; native fixtures use host suffixes or are
guarded where a platform cannot provide their mechanism; downloadable archives
are retested from clean, relocated installations; and the Visual Studio Code
extension is built, validated, and published as a `.vsix`.

Package registries and remote dependencies, lockfiles, workspaces, an editor
language server, a formatter, cross-compilation, optimization controls, source
debug information, a stable general Flecha ABI, and self-hosting remain outside
the 0.1 release contract. The current language and backend limits are documented
by the feature guides linked from the [README](../README.md#documentation-guide).
