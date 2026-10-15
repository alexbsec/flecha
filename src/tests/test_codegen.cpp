#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include "codegen/llvm_code_generator.hpp"
#include "core/parser.hpp"
#include "core/tokenizer.hpp"
#include "lowering/lowerer.hpp"
#include "semantic/semantic_analyzer.hpp"

using namespace flecha::codegen;
using namespace flecha::core;
using namespace flecha::diagnostics;
using namespace flecha::lowering;
using namespace flecha::semantic;

namespace {

struct GeneratedSource final {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit;
    SemanticModel semanticModel;
    std::unique_ptr<BoundProgram> boundProgram;
    std::unique_ptr<LlvmCodeGenerator> generator;
    std::unique_ptr<llvm::Module> module;
};

GeneratedSource Generate(const std::string& source) {
    GeneratedSource result;
    Tokenizer tokenizer(source, result.diagnostics);
    std::vector<Token> tokens = tokenizer.Tokenize();
    Parser parser(std::move(tokens), result.diagnostics);
    result.unit = parser.Parse();
    SemanticAnalyzer analyzer(result.diagnostics);
    result.semanticModel = analyzer.Analyze(*result.unit);
    Lowerer lowerer;
    result.boundProgram = lowerer.Lower(
        *result.unit, result.semanticModel, result.diagnostics);
    if (result.boundProgram != nullptr) {
        result.generator = std::make_unique<LlvmCodeGenerator>();
        result.module = result.generator->Generate(*result.boundProgram,
                                                   "test_module");
    }
    return result;
}

std::string ModuleText(const llvm::Module& module) {
    std::string text;
    llvm::raw_string_ostream output(text);
    module.print(output, nullptr);
    output.flush();
    return text;
}

}  // namespace

TEST(LlvmCodeGeneratorTests, EmitsVerifiedFunctionsLocalsCallsAndReturns) {
    GeneratedSource result = Generate(
        "i32 add(i32 left, i32 right) { return left + right; } "
        "i32 main() { x := 20; y := 22; return add(x, y); }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("define i32 @add(i32 %left, i32 %right)"),
              std::string::npos);
    EXPECT_NE(ir.find("%x = alloca i32"), std::string::npos);
    EXPECT_NE(ir.find("call i32 @add"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsAssignmentsAndConditionalControlFlow) {
    GeneratedSource result = Generate(
        "i32 main() { value := 1; if value < 2 { value = value + 41; } "
        "else { value = 0; } return value; }");

    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("br i1"), std::string::npos);
    EXPECT_NE(ir.find("if.then"), std::string::npos);
    EXPECT_NE(ir.find("if.else"), std::string::npos);
    EXPECT_NE(ir.find("store i32"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsShortCircuitBooleanLogic) {
    GeneratedSource result = Generate(
        "bool both(bool left, bool right) { return left and right; }");

    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("logic.right"), std::string::npos);
    EXPECT_NE(ir.find("phi i1"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, RejectsDeferredLiteralRepresentations) {
    GeneratedSource result = Generate(
        "i32 main() { text := \"hello\"; return 0; }");

    ASSERT_NE(result.generator, nullptr);
    EXPECT_EQ(result.module, nullptr);
    EXPECT_NE(result.generator->Error().find("string"), std::string::npos);
}
