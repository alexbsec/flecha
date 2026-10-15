#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/parser.hpp"
#include "core/tokenizer.hpp"
#include "lowering/bound_ir_dumper.hpp"
#include "lowering/lowerer.hpp"
#include "semantic/semantic_analyzer.hpp"

using namespace flecha::core;
using namespace flecha::diagnostics;
using namespace flecha::lowering;
using namespace flecha::semantic;

namespace {

struct LoweredSource final {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit;
    SemanticModel semanticModel;
    std::unique_ptr<BoundProgram> program;
};

LoweredSource Lower(const std::string& source) {
    LoweredSource result;
    Tokenizer tokenizer(source, result.diagnostics);
    std::vector<Token> tokens = tokenizer.Tokenize();
    Parser parser(std::move(tokens), result.diagnostics);
    result.unit = parser.Parse();
    SemanticAnalyzer analyzer(result.diagnostics);
    result.semanticModel = analyzer.Analyze(*result.unit);
    Lowerer lowerer;
    result.program =
        lowerer.Lower(*result.unit, result.semanticModel, result.diagnostics);
    return result;
}

}  // namespace

TEST(LoweringTests, ResolvesDeclarationsNamesAssignmentsAndCalls) {
    LoweredSource result = Lower(
        "i32 add(i32 left, i32 right) { return left + right; } "
        "i32 main() { value := add(20, 22); value = value + 1; "
        "return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    ASSERT_EQ(result.program->Functions().size(), 2);
    const BoundFunction& mainFunction = *result.program->Functions()[1];
    ASSERT_EQ(mainFunction.Body().Statements().size(), 3);

    const BoundVariableDeclarationStatement& declaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *mainFunction.Body().Statements()[0]);
    EXPECT_EQ(declaration.Variable().Name(), "value");
    EXPECT_EQ(declaration.Variable().Type().Kind(), SemanticTypeKind::I32);
    ASSERT_NE(declaration.Initializer(), nullptr);
    ASSERT_EQ(declaration.Initializer()->Kind(), BoundNodeKind::CallExpression);
    const BoundCallExpression& call =
        static_cast<const BoundCallExpression&>(*declaration.Initializer());
    EXPECT_EQ(call.Function().Name(), "add");
    EXPECT_EQ(call.Arguments().size(), 2);

    const BoundExpressionStatement& expressionStatement =
        static_cast<const BoundExpressionStatement&>(
            *mainFunction.Body().Statements()[1]);
    const BoundAssignmentExpression& assignment =
        static_cast<const BoundAssignmentExpression&>(
            expressionStatement.Expression());
    EXPECT_EQ(static_cast<const BoundVariableExpression&>(assignment.Target())
                  .Symbol()
                  .Name(),
              "value");
    EXPECT_EQ(assignment.Value().Type().Kind(), SemanticTypeKind::I32);
}

TEST(LoweringTests, NormalizesElseIfAndAlsoToNestedConditionals) {
    LoweredSource result = Lower(
        "i32 main() { value := 0; if true { value = 1; } "
        "also false { value = 2; } else if false { value = 3; } "
        "else { value = 4; } return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    const BoundFunction& function = *result.program->Functions()[0];
    const BoundIfStatement& primary =
        static_cast<const BoundIfStatement&>(*function.Body().Statements()[1]);
    ASSERT_EQ(primary.ThenBlock().Statements().size(), 2);
    EXPECT_EQ(primary.ThenBlock().Statements()[1]->Kind(),
              BoundNodeKind::IfStatement);
    ASSERT_NE(primary.ElseBlock(), nullptr);
    ASSERT_EQ(primary.ElseBlock()->Statements().size(), 1);
    const BoundIfStatement& elseIf = static_cast<const BoundIfStatement&>(
        *primary.ElseBlock()->Statements()[0]);
    ASSERT_NE(elseIf.ElseBlock(), nullptr);
    EXPECT_EQ(elseIf.ElseBlock()->Statements().size(), 1);
}

TEST(LoweringTests, NormalizesWhileAlsoInsideLoopBody) {
    LoweredSource result = Lower(
        "i32 main() { value := 0; while value < 2 { value = value + 1; } "
        "also value == 1 { value = value + 10; } return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    const BoundFunction& function = *result.program->Functions()[0];
    const BoundWhileStatement& whileStatement =
        static_cast<const BoundWhileStatement&>(
            *function.Body().Statements()[1]);
    EXPECT_EQ(whileStatement.Kind(), BoundNodeKind::WhileStatement);
    ASSERT_EQ(whileStatement.Body().Statements().size(), 2);
    EXPECT_EQ(whileStatement.Body().Statements()[1]->Kind(),
              BoundNodeKind::IfStatement);
}

TEST(LoweringTests, PreservesEveryCastInAChain) {
    LoweredSource result = Lower(
        "i32 main() { value := 32.cast<i64>.cast<u8> as i32; "
        "return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    const BoundFunction& function = *result.program->Functions()[0];
    const BoundVariableDeclarationStatement& declaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    ASSERT_NE(declaration.Initializer(), nullptr);
    ASSERT_EQ(declaration.Initializer()->Kind(),
              BoundNodeKind::CastExpression);
    const BoundCastExpression& outer =
        static_cast<const BoundCastExpression&>(*declaration.Initializer());
    EXPECT_EQ(outer.Type().Kind(), SemanticTypeKind::I32);
    ASSERT_EQ(outer.Value().Kind(), BoundNodeKind::CastExpression);
    const BoundCastExpression& middle =
        static_cast<const BoundCastExpression&>(outer.Value());
    EXPECT_EQ(middle.Type().Kind(), SemanticTypeKind::U8);
    ASSERT_EQ(middle.Value().Kind(), BoundNodeKind::CastExpression);
    EXPECT_EQ(static_cast<const BoundCastExpression&>(middle.Value())
                  .Type()
                  .Kind(),
              SemanticTypeKind::I64);
}

TEST(LoweringTests, PreservesTypesAndSourceRanges) {
    const std::string source = "i32 main() { return 20 + 22; }";
    LoweredSource result = Lower(source);

    ASSERT_NE(result.program, nullptr);
    const BoundReturnStatement& returnStatement =
        static_cast<const BoundReturnStatement&>(
            *result.program->Functions()[0]->Body().Statements()[0]);
    ASSERT_NE(returnStatement.Expression(), nullptr);
    EXPECT_EQ(returnStatement.Expression()->Type().Kind(),
              SemanticTypeKind::I32);
    EXPECT_EQ(returnStatement.Expression()->Range().Length(), 7);
}

TEST(LoweringTests, RejectsProgramsWithSemanticErrors) {
    LoweredSource result = Lower("i32 main() { missing = 1; return false; }");

    EXPECT_TRUE(result.diagnostics.HasErrors());
    EXPECT_EQ(result.program, nullptr);
}

TEST(LoweringTests, DumpsDeterministicHumanReadableIr) {
    LoweredSource result = Lower(
        "i32 add(i32 left, i32 right) { return left + right; } "
        "i32 main() { answer := add(20, 22); return answer; }");

    ASSERT_NE(result.program, nullptr);
    const std::string dump = DumpBoundProgram(*result.program);
    EXPECT_NE(dump.find("function add(i32 left, i32 right) -> i32"),
              std::string::npos);
    EXPECT_NE(dump.find("local answer: i32 = (call add 20:i32 22:i32)"),
              std::string::npos);
    EXPECT_NE(dump.find("return load answer"), std::string::npos);
}

TEST(LoweringTests, NormalizesStringViewMembersAndIndexSyntax) {
    LoweredSource result = Lower(
        "i32 main() { text := \"flecha\"; first := text.at(0); "
        "same := text[0]; part := text.substr(1, 3); length := text.len(); "
        "return 0; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const std::string dump = DumpBoundProgram(*result.program);
    EXPECT_NE(dump.find("(string.at"), std::string::npos);
    EXPECT_NE(dump.find("(string.at"), dump.rfind("(string.at"));
    EXPECT_NE(dump.find("(string.substr"), std::string::npos);
    EXPECT_NE(dump.find("(string.len"), std::string::npos);

    const BoundFunction& function = *result.program->Functions()[0];
    const BoundVariableDeclarationStatement& atDeclaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *function.Body().Statements()[1]);
    const BoundVariableDeclarationStatement& indexDeclaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *function.Body().Statements()[2]);
    const BoundStringViewOperationExpression& at =
        static_cast<const BoundStringViewOperationExpression&>(
            *atDeclaration.Initializer());
    const BoundStringViewOperationExpression& index =
        static_cast<const BoundStringViewOperationExpression&>(
            *indexDeclaration.Initializer());
    EXPECT_EQ(at.Operation(), StringViewOperation::At);
    EXPECT_EQ(index.Operation(), StringViewOperation::At);
}

TEST(LoweringTests, PreservesOwningStringConversions) {
    LoweredSource result = Lower(
        "string Build() { string owned = \"hi mom\".string(); "
        "string copy = owned.copy(); view := copy.view(); return copy; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const std::string dump = DumpBoundProgram(*result.program);
    EXPECT_NE(dump.find("(string.string"), std::string::npos);
    EXPECT_NE(dump.find("(string.copy"), std::string::npos);
    EXPECT_NE(dump.find("(string.view"), std::string::npos);
}

TEST(LoweringTests, SafelyRepresentsFunctionReferences) {
    LoweredSource result =
        Lower("i32 helper() { return 1; } i32 main() { helper; return 0; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const BoundExpressionStatement& statement =
        static_cast<const BoundExpressionStatement&>(
            *result.program->Functions()[1]->Body().Statements()[0]);
    EXPECT_EQ(statement.Expression().Kind(), BoundNodeKind::FunctionExpression);
}

TEST(LoweringTests, PreservesDelayedDeclarationsAcrossTerminatingBranches) {
    LoweredSource result = Lower(
        "i32 main(bool c) { i32 value; string text; "
        "if c { return 0; } else { value = 42; text = \"ready\"; } "
        "stdoutln(text); return value; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const BoundFunction& function = *result.program->Functions()[0];
    for (std::size_t index = 0; index < 2; ++index) {
        const auto& declaration =
            static_cast<const BoundVariableDeclarationStatement&>(
                *function.Body().Statements()[index]);
        EXPECT_EQ(declaration.Initializer(), nullptr);
    }
    const auto& branch =
        static_cast<const BoundIfStatement&>(*function.Body().Statements()[2]);
    ASSERT_NE(branch.ElseBlock(), nullptr);
    const auto& store = static_cast<const BoundExpressionStatement&>(
        *branch.ElseBlock()->Statements()[0]);
    const auto& assignment =
        static_cast<const BoundAssignmentExpression&>(store.Expression());
    const auto& declaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    EXPECT_EQ(&static_cast<const BoundVariableExpression&>(assignment.Target())
                   .Symbol(),
              &declaration.Variable());
}

TEST(LoweringTests, PreservesConstDeclarationsButReadsOrdinaryValues) {
    LoweredSource result = Lower(
        "const i32 f(const i32 input) { const i32 answer = input; "
        "return answer; } i32 main() { return f(42); }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const std::string dump = DumpBoundProgram(*result.program);
    EXPECT_NE(dump.find("function f(const i32 input) -> const i32"),
              std::string::npos);
    EXPECT_NE(dump.find("local answer: const i32 = load input"),
              std::string::npos);
    const BoundVariableDeclarationStatement& declaration =
        static_cast<const BoundVariableDeclarationStatement&>(
            *result.program->Functions()[0]->Body().Statements()[0]);
    EXPECT_TRUE(declaration.Variable().Type().IsConst());
    EXPECT_FALSE(declaration.Variable().Writable());
    EXPECT_FALSE(declaration.Initializer()->Type().IsConst());
}

TEST(LoweringTests, StopsUnsupportedCompoundTypesBeforeCodeGeneration) {
    for (const std::string& source :
         {"void f() { constref u8 p; }", "void f(&const u8 r) {}", "&u8 f() {}",
          "void f() { 42 as &u8; }"}) {
        SCOPED_TRACE(source);
        LoweredSource result = Lower(source);
        EXPECT_TRUE(result.diagnostics.HasErrors());
        EXPECT_EQ(result.program, nullptr);
    }
}

TEST(LoweringTests, GivesNoneInitializersTheirDeclaredValueType) {
    LoweredSource result = Lower(
        "void f() { const i32 zero = (none); const stringv word = \"none\"; }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const auto& statements =
        result.program->Functions()[0]->Body().Statements();
    const BoundLiteralExpression& zero =
        static_cast<const BoundLiteralExpression&>(
            *static_cast<const BoundVariableDeclarationStatement&>(
                 *statements[0])
                 .Initializer());
    EXPECT_TRUE(zero.IsZeroValue());
    EXPECT_EQ(zero.Type(), SemanticType(SemanticTypeKind::I32));
    const BoundLiteralExpression& word =
        static_cast<const BoundLiteralExpression&>(
            *static_cast<const BoundVariableDeclarationStatement&>(
                 *statements[1])
                 .Initializer());
    EXPECT_FALSE(word.IsZeroValue());
    EXPECT_EQ(word.Value(), "none");
}

TEST(LoweringTests, RepresentsDirectAndIndirectStorageLocations) {
    LoweredSource result = Lower(
        "unsafe void f(*i32 p) { (*p) = 42; *i32 same = &raw *p; } "
        "void g() { i32 x = 0; unsafe { *i32 p = &raw x; f(p); } }");
    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    EXPECT_TRUE(result.program->Functions()[0]->Symbol().IsUnsafe());
    const auto& statement = static_cast<const BoundExpressionStatement&>(
        *result.program->Functions()[0]->Body().Statements()[0]);
    const auto& assignment =
        static_cast<const BoundAssignmentExpression&>(statement.Expression());
    EXPECT_EQ(assignment.Target().Kind(), BoundNodeKind::DereferenceExpression);
    EXPECT_EQ(assignment.Target().Type().Kind(), SemanticTypeKind::I32);
    const std::string dump = DumpBoundProgram(*result.program);
    EXPECT_NE(dump.find("unsafe function f(*i32 p)"), std::string::npos);
    EXPECT_NE(dump.find("(store (deref load p) 42:i32)"), std::string::npos);
    EXPECT_NE(dump.find("(address (deref load p))"), std::string::npos);
    EXPECT_NE(dump.find("(address load x)"), std::string::npos);
}
