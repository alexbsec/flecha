#include <gtest/gtest.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "codegen/llvm_code_generator.hpp"
#include "codegen/native_compiler.hpp"
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
    result.boundProgram =
        lowerer.Lower(*result.unit, result.semanticModel, result.diagnostics);
    if (result.boundProgram != nullptr) {
        result.generator = std::make_unique<LlvmCodeGenerator>();
        result.module =
            result.generator->Generate(*result.boundProgram, "test_module");
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

TEST(LlvmCodeGeneratorTests, EmitsForwardAndRecursiveCalls) {
    GeneratedSource result = Generate(
        "i32 main() { return Add(Factorial(3), 36); } "
        "i32 Add(i32 left, i32 right) { return left + right; } "
        "i32 Factorial(i32 value) { if value <= 1 { return 1; } "
        "return value * Factorial(value - 1); }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("call i32 @Add"), std::string::npos);
    EXPECT_NE(ir.find("call i32 @Factorial"), std::string::npos);
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

TEST(LlvmCodeGeneratorTests, EmitsWhileAndConditionalAlsoControlFlow) {
    GeneratedSource result = Generate(
        "i32 main() { value := 0; counter := 0; while counter < 4 { "
        "value = value + 10; counter = counter + 1; } "
        "also counter % 2 == 0 { value = value + 1; } return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("while.condition"), std::string::npos);
    EXPECT_NE(ir.find("while.body"), std::string::npos);
    EXPECT_NE(ir.find("while.end"), std::string::npos);
    EXPECT_NE(ir.find("if.then"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsLoopControlAndCrossNestedJumps) {
    GeneratedSource result = Generate(
        "i32 main() { value := 0; warmup := 0; while warmup < 1 { "
        "warmup = warmup + 1; proceed; } while true { if value == 0 { "
        "jumpto inner_position; } #outer_position break; while true { "
        "#inner_position value = 42; jumpto outer_position; } } "
        "return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("position.outer_position"), std::string::npos);
    EXPECT_NE(ir.find("position.inner_position"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsChainedNumericConversions) {
    GeneratedSource result = Generate(
        "i32 main() { base := 42; wide := base.cast<i64>; "
        "narrow := wide.cast<u8>; "
        "decimal := narrow.cast<f64>; result := (decimal as f32) as i32; "
        "return result; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("sext i32"), std::string::npos);
    EXPECT_NE(ir.find("trunc i64"), std::string::npos);
    EXPECT_NE(ir.find("uitofp i8"), std::string::npos);
    EXPECT_NE(ir.find("fptrunc double"), std::string::npos);
    EXPECT_NE(ir.find("fptosi float"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsShortCircuitBooleanLogic) {
    GeneratedSource result =
        Generate("bool both(bool left, bool right) { return left and right; }");

    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("logic.right"), std::string::npos);
    EXPECT_NE(ir.find("phi i1"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsStringViewsAndContentEquality) {
    GeneratedSource result = Generate(
        "stringv Identity(stringv value) { return value; } "
        "bool Equal(stringv left, stringv right) { return left == right; } "
        "i32 main() { value := Identity(\"Flecha\"); "
        "if Equal(value, \"Flecha\") and value != \"Arrow\" { return 42; } "
        "return 0; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("@memcmp"), std::string::npos);
    EXPECT_NE(ir.find("stringv.content"), std::string::npos);
    EXPECT_NE(ir.find("{ ptr, i64 }"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsCheckedStringViewOperations) {
    GeneratedSource result = Generate(
        "i32 main() { text := \"flecha\"; first := text.at(0); "
        "last := text[5]; part := text.substr(1, 3); length := text.len(); "
        "return 0; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("@flecha_runtime_v1_string_view_at"),
              std::string::npos);
    EXPECT_NE(ir.find("@flecha_runtime_v1_string_view_check_substr"),
              std::string::npos);
    EXPECT_NE(ir.find("getelementptr inbounds i8"), std::string::npos);
    EXPECT_NE(ir.find("stringv.len"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsOwnedStringConversionsAndCleanup) {
    GeneratedSource result = Generate(
        "i32 main() { string greeting = \"hi mom\".string(); "
        "string copy = greeting; string answer = 42.string(); "
        "stringv view = greeting.view(); "
        "stdoutln(greeting); return 42; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("@flecha_runtime_v1_string_from_view"),
              std::string::npos);
    EXPECT_NE(ir.find("@flecha_runtime_v1_string_from_i32"),
              std::string::npos);
    EXPECT_NE(ir.find("@flecha_runtime_v1_string_destroy"),
              std::string::npos);
    EXPECT_NE(ir.find("@flecha_runtime_v1_stdout_string_view"),
              std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsLengthAwareStandardOutput) {
    GeneratedSource result =
        Generate("i32 main() { stdout(\"Flecha\"); "
                 "stdoutln(\"Language\"); stdout(-42); stdout(1 as i8); "
                 "stdout(2 as i16); stdout(3 as i64); stdout(4 as isize); "
                 "stdout(5 as u8); stdout(6 as u16); stdout(7 as u32); "
                 "stdout(8 as u64); stdout(9 as usize); stdout(1 as f32); "
                 "stdout(1.5); stdout(true); stdout('x'); return 42; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    const std::string suffixes[] = {
        "string_view", "i8",    "i16",  "i32",  "i64",
        "isize",       "u8",    "u16",  "u32",  "u64",
        "usize",       "f32",   "f64",  "bool", "char",
        "newline"};
    for (const std::string& suffix : suffixes) {
        EXPECT_NE(ir.find("@flecha_runtime_v1_stdout_" + suffix),
                  std::string::npos)
            << suffix;
    }
    EXPECT_EQ(ir.find("@write"), std::string::npos);
    EXPECT_EQ(ir.find("@snprintf"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, UsesTargetPointerWidthForSizeTypes) {
    GeneratedSource result =
        Generate("isize identity(isize value) { return value; }");
    ASSERT_NE(result.boundProgram, nullptr);

    LlvmCodeGenerator generator;
    generator.ConfigureTarget("i386-unknown-linux-gnu",
                              llvm::DataLayout("e-p:32:32"));
    std::unique_ptr<llvm::Module> module =
        generator.Generate(*result.boundProgram, "pointer_width_test");
    ASSERT_NE(module, nullptr) << generator.Error();
    const std::string ir = ModuleText(*module);
    EXPECT_NE(ir.find("define i32 @identity(i32 %value)"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, UsesTargetPointerWidthForSizeCasts) {
    GeneratedSource result =
        Generate("usize Convert(i64 value) { return value.cast<usize>; }");
    ASSERT_NE(result.boundProgram, nullptr);

    LlvmCodeGenerator generator;
    generator.ConfigureTarget("i386-unknown-linux-gnu",
                              llvm::DataLayout("e-p:32:32"));
    std::unique_ptr<llvm::Module> module =
        generator.Generate(*result.boundProgram, "size_cast_test");
    ASSERT_NE(module, nullptr) << generator.Error();
    const std::string ir = ModuleText(*module);
    EXPECT_NE(ir.find("trunc i64"), std::string::npos);
    EXPECT_NE(ir.find("to i32"), std::string::npos);
}

TEST(NativeCompilerTests, EmitsObjectAndLinksExecutableForHost) {
    GeneratedSource result = Generate(
        "i32 add(i32 left, i32 right) { return left + right; } "
        "i32 main() { return add(20, 22); }");
    ASSERT_NE(result.boundProgram, nullptr);

    const std::filesystem::path temporaryDirectory =
        std::filesystem::temp_directory_path();
    const std::filesystem::path executablePath =
        temporaryDirectory / "flecha_native_codegen_test";
    const std::filesystem::path objectPath =
        temporaryDirectory / "flecha_native_codegen_test.o";
    std::filesystem::remove(executablePath);
    std::filesystem::remove(objectPath);

    NativeCompiler compiler;
    ASSERT_TRUE(compiler.Ready()) << compiler.Error();
    EXPECT_FALSE(compiler.TargetTriple().empty());
    ASSERT_TRUE(compiler.BuildExecutable(*result.boundProgram, "native_test",
                                         executablePath.string()))
        << compiler.Error();
    EXPECT_TRUE(std::filesystem::is_regular_file(objectPath));
    EXPECT_TRUE(std::filesystem::is_regular_file(executablePath));

    const std::string executablePathText = executablePath.string();
    const llvm::StringRef arguments[] = {executablePathText};
    const int exitCode =
        llvm::sys::ExecuteAndWait(executablePathText, arguments);
    EXPECT_EQ(exitCode, 42);

    std::filesystem::remove(executablePath);
    std::filesystem::remove(objectPath);
}

TEST(NativeCompilerTests, RunsForwardAndRecursiveCalls) {
    GeneratedSource result = Generate(
        "i32 main() { return Add(Factorial(3), 36); } "
        "i32 Add(i32 left, i32 right) { return left + right; } "
        "i32 Factorial(i32 value) { if value <= 1 { return 1; } "
        "return value * Factorial(value - 1); }");
    ASSERT_NE(result.boundProgram, nullptr);

    const std::filesystem::path temporaryDirectory =
        std::filesystem::temp_directory_path();
    const std::filesystem::path executablePath =
        temporaryDirectory / "flecha_native_functions_test";
    const std::filesystem::path objectPath =
        temporaryDirectory / "flecha_native_functions_test.o";
    std::filesystem::remove(executablePath);
    std::filesystem::remove(objectPath);

    NativeCompiler compiler;
    ASSERT_TRUE(compiler.Ready()) << compiler.Error();
    ASSERT_TRUE(compiler.BuildExecutable(
        *result.boundProgram, "native_functions_test", executablePath.string()))
        << compiler.Error();

    const std::string executablePathText = executablePath.string();
    const llvm::StringRef arguments[] = {executablePathText};
    EXPECT_EQ(llvm::sys::ExecuteAndWait(executablePathText, arguments), 42);

    std::filesystem::remove(executablePath);
    std::filesystem::remove(objectPath);
}

TEST(NativeCompilerTests, ObjectEmissionDoesNotRequireRuntimeLibrary) {
    GeneratedSource result =
        Generate("i32 main() { stdoutln(42); return 0; }");
    ASSERT_NE(result.boundProgram, nullptr);

    const std::filesystem::path objectPath =
        std::filesystem::temp_directory_path() /
        "flecha_object_without_runtime_test.o";
    const std::filesystem::path executablePath =
        std::filesystem::temp_directory_path() /
        "flecha_missing_runtime_test";
    std::filesystem::remove(objectPath);
    std::filesystem::remove(executablePath);

    NativeCompiler compiler("/missing/flecha/runtime/library.a");
    ASSERT_TRUE(compiler.EmitObject(*result.boundProgram, "object_only_test",
                                    objectPath.string()))
        << compiler.Error();
    EXPECT_TRUE(std::filesystem::is_regular_file(objectPath));
    EXPECT_FALSE(
        compiler.LinkExecutable(objectPath.string(), executablePath.string()));
    ASSERT_TRUE(compiler.LastDiagnostic().has_value());
    EXPECT_EQ(compiler.LastDiagnostic()->code,
              DiagnosticCode::RuntimeUnavailable);

    std::filesystem::remove(objectPath);
    std::filesystem::remove(executablePath);
}

TEST(NativeCompilerTests, ReportsLinkerFailuresAsStructuredDiagnostics) {
    const std::filesystem::path temporaryDirectory =
        std::filesystem::temp_directory_path();
    const std::filesystem::path missingObject =
        temporaryDirectory / "flecha_intentionally_missing_object.o";
    const std::filesystem::path executablePath =
        temporaryDirectory / "flecha_failed_link_test";
    std::filesystem::remove(missingObject);
    std::filesystem::remove(executablePath);

    NativeCompiler compiler;
    EXPECT_FALSE(compiler.LinkExecutable(missingObject.string(),
                                         executablePath.string()));
    ASSERT_TRUE(compiler.LastDiagnostic().has_value());
    EXPECT_EQ(compiler.LastDiagnostic()->code, DiagnosticCode::LinkerFailed);
    EXPECT_FALSE(std::filesystem::exists(executablePath));
}

TEST(LlvmCodeGeneratorTests,
     EmitsDelayedScalarsAndNullInitializedStringStorage) {
    GeneratedSource result = Generate(
        "i32 main(bool c) { i32 value; string text; "
        "if c { return 0; } else { value = 42; text = \"ready\"; } "
        "stdoutln(text); return value; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("%value = alloca i32"), std::string::npos);
    EXPECT_NE(ir.find("zeroinitializer, ptr %text"), std::string::npos);
    EXPECT_EQ(ir.find("store i32 0, ptr %value"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, CleansLaterStringSlotsAtEarlierBackwardJumpExits) {
    GeneratedSource result = Generate(
        "i32 main() { count := 0; while count < 2 { #again "
        "if count == 1 { return 42; } count = count + 1; "
        "string late = \"owned\"; jumpto again; } return 0; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const llvm::Function* function = result.module->getFunction("main");
    ASSERT_NE(function, nullptr);
    bool foundReturn = false;
    for (const llvm::BasicBlock& block : *function) {
        const auto* terminator =
            llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator());
        if (terminator == nullptr) continue;
        const auto* value =
            llvm::dyn_cast<llvm::ConstantInt>(terminator->getReturnValue());
        if (value == nullptr || value->getSExtValue() != 42) continue;
        foundReturn = true;
        bool cleansLate = false;
        for (const llvm::Instruction& instruction : block) {
            const auto* store = llvm::dyn_cast<llvm::StoreInst>(&instruction);
            if (store != nullptr &&
                store->getPointerOperand()->getName() == "late") {
                cleansLate = llvm::isa<llvm::ConstantAggregateZero>(
                    store->getValueOperand());
            }
        }
        EXPECT_TRUE(cleansLate);
    }
    EXPECT_TRUE(foundReturn);
}

TEST(LlvmCodeGeneratorTests, EmitsRuntimeInitializedConstValuesAndParameters) {
    GeneratedSource result = Generate(
        "const i32 add(const i32 input) { return input + 2; } "
        "i32 main() { const i32 value = add(40); copy := value; "
        "copy = copy + 1; const i64 wide = value.cast<const i64>; "
        "const bool same = value == 42; const i32 zero = none; "
        "const string text = \"hi\"; const stringv view = text.view(); "
        "stdoutln(view); return copy - 1 + zero; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("define i32 @add(i32 %input)"), std::string::npos);
    EXPECT_NE(ir.find("call i32 @add(i32 40)"), std::string::npos);
}

TEST(LlvmCodeGeneratorTests, EmitsTypedZeroValuesForConstInitializers) {
    GeneratedSource result = Generate(
        "const i32 zero() { return none; } "
        "i32 identity(const i32 value) { return value; } "
        "i32 main() { const bool b = none; const char c = none; "
        "const f64 f = none; const string s = none; const stringv v = none; "
        "i32 mutableZero = 1; mutableZero = none; "
        "const i32 result = identity((none)) + zero(); return result; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
}

TEST(LlvmCodeGeneratorTests,
     EmitsRawAddressesIndirectAccessAndPointerEquality) {
    GeneratedSource result = Generate(
        "unsafe i32 read(*const i32 p) { return *p; } "
        "i32 main() { i32 value = 40; *i32 p = null; "
        "unsafe { p = &raw value; (*p) = *p + 2; } "
        "if p != none and null != p { unsafe { return read(p); } } return 0; "
        "}");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.module, nullptr) << result.generator->Error();
    EXPECT_TRUE(VerifyModule(*result.module).empty());
    const std::string ir = ModuleText(*result.module);
    EXPECT_NE(ir.find("define i32 @read(ptr %p)"), std::string::npos);
    EXPECT_NE(ir.find("icmp ne ptr"), std::string::npos);
    EXPECT_NE(ir.find("load i32, ptr"), std::string::npos);
}
