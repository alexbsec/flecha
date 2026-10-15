# Native code generation

The native compiler selects LLVM's host target triple and creates a
`TargetMachine` for that target. Its data layout is attached to the generated
module before IR construction, so `isize` and `usize` use the target pointer
width rather than a compiler-host constant.

`NativeCompiler::EmitObject` runs LLVM's legacy code-emission pass pipeline and
writes a relocatable object. `NativeCompiler::LinkExecutable` invokes the
system `cc` driver without a shell, which supplies the platform startup objects
and standard executable-linking conventions. `BuildExecutable` retains the
intermediate object beside the executable as `<output>.o`.

Commands:

```sh
flecha --emit-object program.fl -o program.o
flecha build program.fl -o program
```

The current implementation targets the machine running the compiler. Explicit
cross-compilation triples, sysroots, runtime libraries, optimization levels,
and debug information are future work.

Run the native end-to-end proof with:

```sh
make native-poc
```

It compiles `examples/llvm_poc.fl`, confirms that both the object and executable
were produced, executes the result, and requires exit code `42`.
