#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/parser.hpp"
#include "core/tokenizer.hpp"
#include "lowering/lowerer.hpp"
#include "lowering/bound_ir_dumper.hpp"
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
    result.program = lowerer.Lower(*result.unit, result.semanticModel,
                                   result.diagnostics);
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
    ASSERT_EQ(declaration.Initializer().Kind(), BoundNodeKind::CallExpression);
    const BoundCallExpression& call =
        static_cast<const BoundCallExpression&>(declaration.Initializer());
    EXPECT_EQ(call.Function().Name(), "add");
    EXPECT_EQ(call.Arguments().size(), 2);

    const BoundExpressionStatement& expressionStatement =
        static_cast<const BoundExpressionStatement&>(
            *mainFunction.Body().Statements()[1]);
    const BoundAssignmentExpression& assignment =
        static_cast<const BoundAssignmentExpression&>(
            expressionStatement.Expression());
    EXPECT_EQ(assignment.Target().Name(), "value");
    EXPECT_EQ(assignment.Value().Type().Kind(), SemanticTypeKind::I32);
}

TEST(LoweringTests, NormalizesElseIfAndAlsoToNestedConditionals) {
    LoweredSource result = Lower(
        "i32 main() { value := 0; if true { value = 1; } "
        "also false { value = 2; } else if false { value = 3; } "
        "else { value = 4; } return value; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    const BoundFunction& function = *result.program->Functions()[0];
    const BoundIfStatement& primary = static_cast<const BoundIfStatement&>(
        *function.Body().Statements()[1]);
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
    LoweredSource result = Lower(
        "i32 main() { missing = 1; return false; }");

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

TEST(LoweringTests, SafelyRepresentsFunctionReferences) {
    LoweredSource result = Lower(
        "i32 helper() { return 1; } i32 main() { helper; return 0; }");

    ASSERT_FALSE(result.diagnostics.HasErrors());
    ASSERT_NE(result.program, nullptr);
    const BoundExpressionStatement& statement =
        static_cast<const BoundExpressionStatement&>(
            *result.program->Functions()[1]->Body().Statements()[0]);
    EXPECT_EQ(statement.Expression().Kind(), BoundNodeKind::FunctionExpression);
}
