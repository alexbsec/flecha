#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include "codegen/llvm_code_generator.hpp"
#include "codegen/native_compiler.hpp"
#include "core/parser.hpp"
#include "core/tokenizer.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/diagnostic_bag.hpp"
#include "lowering/lowerer.hpp"
#include "semantic/semantic_analyzer.hpp"

namespace {

void PrintDiagnostics(
    const flecha::diagnostics::DiagnosticBag& diagnostics) {
    for (const flecha::diagnostics::Diagnostic& diagnostic :
         diagnostics.Diagnostics()) {
        std::cerr << flecha::diagnostics::DiagnosticCodeName(diagnostic.code)
                  << ":" << diagnostic.range.start.line << ":"
                  << diagnostic.range.start.column << ": "
                  << diagnostic.message << "\n";
    }
}

void PrintDiagnostic(const flecha::diagnostics::Diagnostic& diagnostic) {
    std::cerr << flecha::diagnostics::DiagnosticCodeName(diagnostic.code)
              << ":" << diagnostic.range.start.line << ":"
              << diagnostic.range.start.column << ": " << diagnostic.message
              << "\n";
}

int EmitLlvm(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream input(inputPath);
    if (!input) {
        std::cerr << "unable to open input file '" << inputPath << "'\n";
        return 1;
    }
    std::ostringstream sourceBuffer;
    sourceBuffer << input.rdbuf();
    const std::string source = sourceBuffer.str();

    flecha::diagnostics::DiagnosticBag diagnostics;
    flecha::core::Tokenizer tokenizer(source, diagnostics);
    std::vector<flecha::core::Token> tokens = tokenizer.Tokenize();
    flecha::core::Parser parser(std::move(tokens), diagnostics);
    std::unique_ptr<flecha::core::CompilationUnit> unit = parser.Parse();
    flecha::semantic::SemanticAnalyzer analyzer(diagnostics);
    flecha::semantic::SemanticModel semanticModel = analyzer.Analyze(*unit);
    flecha::lowering::Lowerer lowerer;
    std::unique_ptr<flecha::lowering::BoundProgram> boundProgram =
        lowerer.Lower(*unit, semanticModel, diagnostics);
    if (boundProgram == nullptr) {
        PrintDiagnostics(diagnostics);
        return 1;
    }

    flecha::codegen::LlvmCodeGenerator generator;
    std::unique_ptr<llvm::Module> module =
        generator.Generate(*boundProgram, inputPath);
    if (module == nullptr) {
        std::cerr << "LLVM generation failed: " << generator.Error() << "\n";
        return 1;
    }

    if (outputPath.empty()) {
        module->print(llvm::outs(), nullptr);
        return 0;
    }
    std::error_code error;
    llvm::raw_fd_ostream output(outputPath, error);
    if (error) {
        std::cerr << "unable to open output file '" << outputPath
                  << "': " << error.message() << "\n";
        return 1;
    }
    module->print(output, nullptr);
    return 0;
}

int BuildNative(const std::string& inputPath, const std::string& outputPath,
                bool objectOnly) {
    std::ifstream input(inputPath);
    if (!input) {
        std::cerr << "unable to open input file '" << inputPath << "'\n";
        return 1;
    }
    std::ostringstream sourceBuffer;
    sourceBuffer << input.rdbuf();
    const std::string source = sourceBuffer.str();

    flecha::diagnostics::DiagnosticBag diagnostics;
    flecha::core::Tokenizer tokenizer(source, diagnostics);
    std::vector<flecha::core::Token> tokens = tokenizer.Tokenize();
    flecha::core::Parser parser(std::move(tokens), diagnostics);
    std::unique_ptr<flecha::core::CompilationUnit> unit = parser.Parse();
    flecha::semantic::SemanticAnalyzer analyzer(diagnostics);
    flecha::semantic::SemanticModel semanticModel = analyzer.Analyze(*unit);
    flecha::lowering::Lowerer lowerer;
    std::unique_ptr<flecha::lowering::BoundProgram> boundProgram =
        lowerer.Lower(*unit, semanticModel, diagnostics);
    if (boundProgram == nullptr) {
        PrintDiagnostics(diagnostics);
        return 1;
    }

    flecha::codegen::NativeCompiler compiler;
    if (!compiler.Ready()) {
        if (compiler.LastDiagnostic().has_value()) {
            PrintDiagnostic(*compiler.LastDiagnostic());
        }
        return 1;
    }
    const bool success =
        objectOnly
            ? compiler.EmitObject(*boundProgram, inputPath, outputPath)
            : compiler.BuildExecutable(*boundProgram, inputPath, outputPath);
    if (!success) {
        if (compiler.LastDiagnostic().has_value()) {
            PrintDiagnostic(*compiler.LastDiagnostic());
        }
        return 1;
    }
    return 0;
}

void PrintUsage(const char* executable) {
    std::cerr << "usage:\n"
              << "  " << executable
              << " --emit-llvm <input.fl> [-o <output.ll>]\n"
              << "  " << executable
              << " --emit-object <input.fl> -o <output.o>\n"
              << "  " << executable
              << " build <input.fl> -o <executable>\n";
}

}  // namespace

int main(int argumentCount, char** arguments) {
    if (argumentCount != 3 && argumentCount != 5) {
        PrintUsage(arguments[0]);
        return 1;
    }
    const std::string command = arguments[1];
    if (command != "--emit-llvm" && command != "--emit-object" &&
        command != "build") {
        PrintUsage(arguments[0]);
        return 1;
    }
    std::string outputPath;
    if (argumentCount == 5) {
        if (std::string(arguments[3]) != "-o") {
            PrintUsage(arguments[0]);
            return 1;
        }
        outputPath = arguments[4];
    }
    if (command == "--emit-llvm") {
        return EmitLlvm(arguments[2], outputPath);
    }
    if (outputPath.empty()) {
        PrintUsage(arguments[0]);
        return 1;
    }
    return BuildNative(arguments[2], outputPath,
                       command == "--emit-object");
}
