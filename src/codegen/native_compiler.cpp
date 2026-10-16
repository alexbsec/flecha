#include "codegen/native_compiler.hpp"

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <llvm/ADT/SmallString.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include "codegen/llvm_code_generator.hpp"

namespace flecha::codegen {

#ifndef FLECHA_RUNTIME_LIBRARY_PATH
#define FLECHA_RUNTIME_LIBRARY_PATH ""
#endif

NativeCompiler::NativeCompiler(std::string runtimeLibraryPath)
    : _runtimeLibraryPath(runtimeLibraryPath.empty()
                              ? FLECHA_RUNTIME_LIBRARY_PATH
                              : std::move(runtimeLibraryPath)) {
    if (llvm::InitializeNativeTarget() ||
        llvm::InitializeNativeTargetAsmPrinter()) {
        Fail(diagnostics::DiagnosticCode::NativeTargetUnavailable,
             "failed to initialize the native LLVM target");
        return;
    }

    _targetTriple = llvm::sys::getDefaultTargetTriple();
    const llvm::Triple triple(_targetTriple);
    std::string lookupError;
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(triple, lookupError);
    if (target == nullptr) {
        Fail(diagnostics::DiagnosticCode::NativeTargetUnavailable,
             "unable to find LLVM target for '" + _targetTriple +
                 "': " + lookupError);
        return;
    }

    llvm::TargetOptions options;
    _targetMachine.reset(target->createTargetMachine(
        triple, "generic", "", options, llvm::Reloc::PIC_));
    if (_targetMachine == nullptr) {
        Fail(diagnostics::DiagnosticCode::NativeTargetUnavailable,
             "unable to create LLVM target machine for '" + _targetTriple +
                 "'");
    }
}

bool NativeCompiler::Fail(diagnostics::DiagnosticCode code,
                          std::string message) {
    _error = std::move(message);
    _diagnostic = diagnostics::Diagnostic{
        code, diagnostics::DiagnosticSeverity::Error, _error,
        source::SourceRange{}};
    return false;
}

bool NativeCompiler::EmitObject(const lowering::BoundProgram& program,
                                const std::string& moduleName,
                                const std::string& objectPath) {
    _error.clear();
    _diagnostic.reset();
    if (_targetMachine == nullptr) {
        return Fail(diagnostics::DiagnosticCode::NativeTargetUnavailable,
                    "native LLVM target is not available");
    }

    LlvmCodeGenerator generator;
    generator.ConfigureTarget(_targetTriple,
                              _targetMachine->createDataLayout());
    std::unique_ptr<llvm::Module> module =
        generator.Generate(program, moduleName);
    if (module == nullptr) {
        return Fail(diagnostics::DiagnosticCode::ObjectEmissionFailed,
                    generator.Error());
    }

    std::error_code fileError;
    llvm::raw_fd_ostream output(objectPath, fileError,
                                llvm::sys::fs::OF_None);
    if (fileError) {
        return Fail(diagnostics::DiagnosticCode::ObjectEmissionFailed,
                    "unable to open object file '" + objectPath +
                        "': " + fileError.message());
    }

    llvm::legacy::PassManager passManager;
    if (_targetMachine->addPassesToEmitFile(
            passManager, output, nullptr,
            llvm::CodeGenFileType::ObjectFile, false)) {
        return Fail(diagnostics::DiagnosticCode::ObjectEmissionFailed,
                    "LLVM target cannot emit object files");
    }
    passManager.run(*module);
    output.flush();
    if (output.has_error()) {
        return Fail(diagnostics::DiagnosticCode::ObjectEmissionFailed,
                    "failed while writing object file '" + objectPath + "'");
    }
    return true;
}

bool NativeCompiler::LinkExecutable(
    const std::string& objectPath, const std::string& outputPath,
    const std::vector<std::string>& linkInputs) {
    _error.clear();
    _diagnostic.reset();
    if (_runtimeLibraryPath.empty() ||
        !llvm::sys::fs::exists(_runtimeLibraryPath)) {
        return Fail(diagnostics::DiagnosticCode::RuntimeUnavailable,
                    "Flecha runtime library was not found at '" +
                        _runtimeLibraryPath + "'");
    }
    const llvm::ErrorOr<std::string> linker =
        llvm::sys::findProgramByName("c++");
    if (!linker) {
        return Fail(diagnostics::DiagnosticCode::LinkerFailed,
                    "unable to find the system C++ linker driver 'c++'");
    }

    std::vector<std::string> ownedArguments{*linker, objectPath};
    ownedArguments.insert(ownedArguments.end(), linkInputs.begin(),
                          linkInputs.end());
    ownedArguments.push_back(_runtimeLibraryPath);
#ifdef FLECHA_SANITIZED_RUNTIME
    ownedArguments.emplace_back("-fsanitize=address,undefined");
#endif
    ownedArguments.emplace_back("-o");
    ownedArguments.push_back(outputPath);
    std::vector<llvm::StringRef> arguments;
    arguments.reserve(ownedArguments.size());
    for (const auto& argument : ownedArguments)
        arguments.emplace_back(argument);
    std::string executionError;
    bool executionFailed = false;
    const int result =
        llvm::sys::ExecuteAndWait(*linker, arguments, std::nullopt, {}, 0, 0,
                                  &executionError, &executionFailed);
    if (executionFailed || result != 0) {
        std::string message =
            "system linker failed with exit code " + std::to_string(result);
        if (!executionError.empty()) message += ": " + executionError;
        return Fail(diagnostics::DiagnosticCode::LinkerFailed,
                    std::move(message));
    }
    return true;
}

bool NativeCompiler::BuildExecutable(
    const lowering::BoundProgram& program, const std::string& moduleName,
    const std::string& outputPath, const std::vector<std::string>& linkInputs) {
    llvm::SmallString<256> objectPath;
    const std::error_code temporaryError = llvm::sys::fs::createUniqueFile(
        outputPath + ".flecha-%%%%%%.o", objectPath);
    if (temporaryError) {
        return Fail(diagnostics::DiagnosticCode::ObjectEmissionFailed,
                    "unable to create temporary object for '" + outputPath +
                        "': " + temporaryError.message());
    }
    llvm::FileRemover removeObject(objectPath);
    const std::string objectPathText = objectPath.str().str();
    if (!EmitObject(program, moduleName, objectPathText)) return false;
    return LinkExecutable(objectPathText, outputPath, linkInputs);
}

}  // namespace flecha::codegen
