#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "codegen/llvm_code_generator.hpp"
#include "codegen/native_compiler.hpp"
#include "core/module_loader.hpp"
#include "flecha/version.hpp"
#include "lowering/lowerer.hpp"
#include "semantic/semantic_analyzer.hpp"

namespace {
void PrintDiagnostic(const flecha::diagnostics::Diagnostic& diagnostic) {
    if (!diagnostic.range.start.file.empty())
        std::cerr << diagnostic.range.start.file << ":";
    std::cerr << flecha::diagnostics::DiagnosticCodeName(diagnostic.code) << ":"
              << diagnostic.range.start.line << ":"
              << diagnostic.range.start.column << ": ";
    if (diagnostic.severity == flecha::diagnostics::DiagnosticSeverity::Warning)
        std::cerr << "warning: ";
    std::cerr << diagnostic.message << "\n";
}
void PrintDiagnostics(const flecha::diagnostics::DiagnosticBag& diagnostics) {
    for (const auto& diagnostic : diagnostics.Diagnostics())
        PrintDiagnostic(diagnostic);
}
int Compile(const std::string& command, const std::string& input,
            const std::string& output, const std::filesystem::path& root,
            const std::map<std::string, std::filesystem::path>& libraries,
            const std::vector<std::string>& linkInputs) {
    flecha::diagnostics::DiagnosticBag diagnostics;
    flecha::core::ModuleLoader loader(diagnostics);
    auto unit = loader.Load(input, root, libraries);
    if (diagnostics.HasErrors()) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    flecha::semantic::SemanticAnalyzer analyzer(diagnostics);
    auto semanticModel = analyzer.Analyze(*unit);
    if (diagnostics.HasErrors()) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    PrintDiagnostics(diagnostics);
    if (command == "build") {
        const flecha::semantic::FunctionSymbol* entry = nullptr;
        for (const auto& symbol : semanticModel.Symbols())
            if (symbol->Kind() == flecha::semantic::SymbolKind::Function &&
                symbol->Name() == "main")
                entry = static_cast<const flecha::semantic::FunctionSymbol*>(
                    symbol.get());
        if (!entry || !entry->ParameterTypes().empty() ||
            (entry->ReturnType().Kind() !=
                 flecha::semantic::SemanticTypeKind::I32 &&
             entry->ReturnType().Kind() !=
                 flecha::semantic::SemanticTypeKind::Void)) {
            std::cerr << "an executable requires i32 main() or void main() in "
                         "its entry module\n";
            return 1;
        }
    }
    if (command == "check") return 0;
    flecha::lowering::Lowerer lowerer;
    auto program = lowerer.Lower(*unit, semanticModel, diagnostics);
    if (!program) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    if (command == "--emit-llvm") {
        flecha::codegen::LlvmCodeGenerator generator;
        auto module = generator.Generate(*program, input);
        if (!module) {
            std::cerr << "LLVM generation failed: " << generator.Error()
                      << "\n";
            return 1;
        }
        if (output.empty()) {
            module->print(llvm::outs(), nullptr);
            return 0;
        }
        std::error_code error;
        llvm::raw_fd_ostream stream(output, error);
        if (error) {
            std::cerr << "cannot write '" << output << "': " << error.message()
                      << "\n";
            return 1;
        }
        module->print(stream, nullptr);
        return 0;
    }
    flecha::codegen::NativeCompiler compiler;
    const bool success =
        compiler.Ready() &&
        (command == "--emit-object"
             ? compiler.EmitObject(*program, input, output)
             : compiler.BuildExecutable(*program, input, output, linkInputs));
    if (!success && compiler.LastDiagnostic())
        PrintDiagnostic(*compiler.LastDiagnostic());
    return success ? 0 : 1;
}
void PrintUsage(const char* executable) {
    std::cerr
        << "usage: " << executable
        << " <check|build|--emit-llvm|--emit-object> <input.fl> [-o output]"
           " [--root source-directory] [--lib name=source-directory ...]"
           " [--link object-or-library ...]\n";
}
}  // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "flecha " << flecha::VERSION << "\n";
        return 0;
    }
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }
    const std::string command = argv[1];
    if (command != "check" && command != "build" && command != "--emit-llvm" &&
        command != "--emit-object") {
        PrintUsage(argv[0]);
        return 1;
    }
    std::string output;
    std::filesystem::path root;
    std::map<std::string, std::filesystem::path> libraries;
    std::vector<std::string> linkInputs;
    for (int index = 3; index < argc; ++index) {
        const std::string option = argv[index];
        if (++index == argc) {
            PrintUsage(argv[0]);
            return 1;
        }
        const std::string value = argv[index];
        if (option == "-o")
            output = value;
        else if (option == "--root")
            root = value;
        else if (option == "--lib") {
            const auto equals = value.find('=');
            if (equals == std::string::npos || equals == 0 ||
                equals + 1 == value.size() ||
                !libraries
                     .emplace(value.substr(0, equals), value.substr(equals + 1))
                     .second) {
                std::cerr << "--lib requires a unique name=source-directory\n";
                return 1;
            }
        } else if (option == "--link") {
            linkInputs.push_back(value);
        } else {
            PrintUsage(argv[0]);
            return 1;
        }
    }
    if ((command == "build" || command == "--emit-object") && output.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (command != "build" && !linkInputs.empty()) {
        std::cerr << "--link is supported only by build\n";
        return 1;
    }
    try {
        return Compile(command, argv[2], output, root, libraries, linkInputs);
    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
