#include "codegen/native_compiler.hpp"

#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include "codegen/llvm_code_generator.hpp"

namespace flecha::codegen {

NativeCompiler::NativeCompiler() {
    if (llvm::InitializeNativeTarget() ||
        llvm::InitializeNativeTargetAsmPrinter()) {
        Fail("failed to initialize the native LLVM target");
        return;
    }

    _targetTriple = llvm::sys::getDefaultTargetTriple();
    const llvm::Triple triple(_targetTriple);
    std::string lookupError;
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(triple, lookupError);
    if (target == nullptr) {
        Fail("unable to find LLVM target for '" + _targetTriple +
             "': " + lookupError);
        return;
    }

    llvm::TargetOptions options;
    _targetMachine.reset(target->createTargetMachine(
        triple, "generic", "", options, llvm::Reloc::PIC_));
    if (_targetMachine == nullptr) {
        Fail("unable to create LLVM target machine for '" + _targetTriple +
             "'");
    }
}

bool NativeCompiler::Fail(std::string message) {
    _error = std::move(message);
    return false;
}

bool NativeCompiler::EmitObject(const lowering::BoundProgram& program,
                                const std::string& moduleName,
                                const std::string& objectPath) {
    _error.clear();
    if (_targetMachine == nullptr) {
        return Fail("native LLVM target is not available");
    }

    LlvmCodeGenerator generator;
    generator.ConfigureTarget(_targetTriple,
                              _targetMachine->createDataLayout());
    std::unique_ptr<llvm::Module> module =
        generator.Generate(program, moduleName);
    if (module == nullptr) {
        return Fail(generator.Error());
    }

    std::error_code fileError;
    llvm::raw_fd_ostream output(objectPath, fileError,
                                llvm::sys::fs::OF_None);
    if (fileError) {
        return Fail("unable to open object file '" + objectPath +
                    "': " + fileError.message());
    }

    llvm::legacy::PassManager passManager;
    if (_targetMachine->addPassesToEmitFile(
            passManager, output, nullptr,
            llvm::CodeGenFileType::ObjectFile, false)) {
        return Fail("LLVM target cannot emit object files");
    }
    passManager.run(*module);
    output.flush();
    if (output.has_error()) {
        return Fail("failed while writing object file '" + objectPath + "'");
    }
    return true;
}

bool NativeCompiler::LinkExecutable(const std::string& objectPath,
                                    const std::string& outputPath) {
    _error.clear();
    const llvm::ErrorOr<std::string> linker =
        llvm::sys::findProgramByName("cc");
    if (!linker) {
        return Fail("unable to find the system C linker driver 'cc'");
    }

    const llvm::StringRef arguments[] = {
        *linker, objectPath, "-o", outputPath};
    std::string executionError;
    bool executionFailed = false;
    const int result = llvm::sys::ExecuteAndWait(
        *linker, arguments, std::nullopt, {}, 0, 0, &executionError,
        &executionFailed);
    if (executionFailed || result != 0) {
        std::string message = "system linker failed with exit code " +
                              std::to_string(result);
        if (!executionError.empty()) message += ": " + executionError;
        return Fail(std::move(message));
    }
    return true;
}

bool NativeCompiler::BuildExecutable(const lowering::BoundProgram& program,
                                     const std::string& moduleName,
                                     const std::string& outputPath) {
    const std::string objectPath = outputPath + ".o";
    if (!EmitObject(program, moduleName, objectPath)) return false;
    return LinkExecutable(objectPath, outputPath);
}

}  // namespace flecha::codegen
