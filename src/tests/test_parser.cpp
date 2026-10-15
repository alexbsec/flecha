#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/parser.hpp"
#include "core/tokenizer.hpp"

using namespace flecha::core;
using namespace flecha::diagnostics;
using namespace flecha::source;

namespace {

std::vector<Token> Lex(const std::string& source, DiagnosticBag& diagnostics) {
    Tokenizer tokenizer(source, diagnostics);
    return tokenizer.Tokenize();
}

std::unique_ptr<CompilationUnit> ParseSource(const std::string& source,
                                             DiagnosticBag& diagnostics) {
    std::vector<Token> tokens = Lex(source, diagnostics);
    Parser parser(std::move(tokens), diagnostics);
    return parser.Parse();
}

const Expression& ReturnedExpression(const CompilationUnit& unit) {
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit.Declarations()[0]);
    const ReturnStatement& statement =
        static_cast<const ReturnStatement&>(*function.Body().Statements()[0]);
    return *statement.Value();
}

}  // namespace

TEST(ParserTests, ParsesEmptyCompilationUnit) {
    DiagnosticBag diagnostics;
    Parser parser({}, diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->Kind(), AstNodeKind::CompilationUnit);
    EXPECT_TRUE(unit->Declarations().empty());
    EXPECT_TRUE(unit->Range().Empty());
    EXPECT_TRUE(diagnostics.Empty());
}

TEST(ParserTests, UsesTokenRangesForCompilationUnit) {
    DiagnosticBag diagnostics;
    std::vector<Token> tokens = Lex("i32", diagnostics);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    EXPECT_EQ(unit->Range().start.offset, 0);
    EXPECT_EQ(unit->Range().end.offset, 3);
}

TEST(ParserTests, ReportsUnexpectedTopLevelTokensWithoutThrowing) {
    DiagnosticBag diagnostics;
    std::vector<Token> tokens = Lex("main other", diagnostics);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit;
    EXPECT_NO_THROW(unit = parser.Parse());

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UnexpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].range.start.offset, 0);
}

TEST(ParserTests, ParsesCompleteFunctionWithReturnStatement) {
    DiagnosticBag diagnostics;
    std::vector<Token> tokens = Lex("i32 main() { return 0; }", diagnostics);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    ASSERT_FALSE(diagnostics.HasErrors());
    ASSERT_EQ(unit->Declarations().size(), 1);
    ASSERT_EQ(unit->Declarations()[0]->Kind(),
              AstNodeKind::FunctionDeclaration);
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    EXPECT_EQ(function.Name(), "main");
    EXPECT_EQ(function.ReturnType().Type(), PrimitiveType::I32);
    EXPECT_TRUE(function.Parameters().empty());
    ASSERT_EQ(function.Body().Statements().size(), 1);
    ASSERT_EQ(function.Body().Statements()[0]->Kind(),
              AstNodeKind::ReturnStatement);
    const ReturnStatement& returnStatement =
        static_cast<const ReturnStatement&>(*function.Body().Statements()[0]);
    ASSERT_NE(returnStatement.Value(), nullptr);
    EXPECT_EQ(returnStatement.Value()->Kind(), AstNodeKind::LiteralExpression);
    EXPECT_EQ(unit->Range().start.offset, 0);
    EXPECT_EQ(unit->Range().end.offset, 24);
}

TEST(ParserTests, ParsesTypedParametersAndBareReturn) {
    DiagnosticBag diagnostics;
    std::vector<Token> tokens =
        Lex("void reset(i32 value, bool enabled) { return; }", diagnostics);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    ASSERT_FALSE(diagnostics.HasErrors());
    ASSERT_EQ(unit->Declarations().size(), 1);
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    ASSERT_EQ(function.Parameters().size(), 2);
    EXPECT_EQ(function.Parameters()[0]->Name(), "value");
    EXPECT_EQ(function.Parameters()[0]->Type().Type(), PrimitiveType::I32);
    EXPECT_EQ(function.Parameters()[1]->Name(), "enabled");
    EXPECT_EQ(function.Parameters()[1]->Type().Type(), PrimitiveType::Bool);
    const ReturnStatement& returnStatement =
        static_cast<const ReturnStatement&>(*function.Body().Statements()[0]);
    EXPECT_EQ(returnStatement.Value(), nullptr);
}

TEST(ParserTests, ReportsMissingSemicolonAndPreservesFunction) {
    DiagnosticBag diagnostics;
    std::vector<Token> tokens = Lex("i32 main() { return 0 }", diagnostics);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code, DiagnosticCode::ExpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].range.start.offset, 22);
    ASSERT_EQ(unit->Declarations().size(), 1);
}

TEST(ParserTests, AppendsMissingEndOfFileTokenSafely) {
    DiagnosticBag diagnostics;
    const SourceRange range{{0, 1, 1}, {3, 1, 4}};
    std::vector<Token> tokens;
    tokens.emplace_back(TokenType::I32, "i32", range);
    Parser parser(std::move(tokens), diagnostics);

    std::unique_ptr<CompilationUnit> unit = parser.Parse();

    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->Range().end.offset, 3);
    EXPECT_EQ(diagnostics.Diagnostics().size(), 1);
}

TEST(ParserExpressionTests, AppliesCompletePrecedenceTable) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("bool test() { return a or b and c ^ d == e < f + g * h; }",
                    diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const BinaryExpression& logicalOr =
        static_cast<const BinaryExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(logicalOr.OperatorType(), TokenType::Or);

    const BinaryExpression& logicalAnd =
        static_cast<const BinaryExpression&>(logicalOr.Right());
    EXPECT_EQ(logicalAnd.OperatorType(), TokenType::And);

    const BinaryExpression& xorExpression =
        static_cast<const BinaryExpression&>(logicalAnd.Right());
    EXPECT_EQ(xorExpression.OperatorType(), TokenType::Xor);

    const BinaryExpression& equality =
        static_cast<const BinaryExpression&>(xorExpression.Right());
    EXPECT_EQ(equality.OperatorType(), TokenType::Equal);

    const BinaryExpression& comparison =
        static_cast<const BinaryExpression&>(equality.Right());
    EXPECT_EQ(comparison.OperatorType(), TokenType::Less);

    const BinaryExpression& addition =
        static_cast<const BinaryExpression&>(comparison.Right());
    EXPECT_EQ(addition.OperatorType(), TokenType::Add);

    const BinaryExpression& multiplication =
        static_cast<const BinaryExpression&>(addition.Right());
    EXPECT_EQ(multiplication.OperatorType(), TokenType::Mul);
}

TEST(ParserExpressionTests, BinaryOperatorsAreLeftAssociative) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return 10 - 3 - 2; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const BinaryExpression& outer =
        static_cast<const BinaryExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(outer.OperatorType(), TokenType::Sub);
    ASSERT_EQ(outer.Left().Kind(), AstNodeKind::BinaryExpression);
    const BinaryExpression& inner =
        static_cast<const BinaryExpression&>(outer.Left());
    EXPECT_EQ(inner.OperatorType(), TokenType::Sub);
}

TEST(ParserExpressionTests, UnaryOperatorsBindTighterAndAssociateRight) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return !-*value; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const UnaryExpression& logicalNot =
        static_cast<const UnaryExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(logicalNot.OperatorType(), TokenType::Bang);
    const UnaryExpression& negation =
        static_cast<const UnaryExpression&>(logicalNot.Operand());
    EXPECT_EQ(negation.OperatorType(), TokenType::Sub);
    const UnaryExpression& dereference =
        static_cast<const UnaryExpression&>(negation.Operand());
    EXPECT_EQ(dereference.OperatorType(), TokenType::Mul);
}

TEST(ParserExpressionTests, ParenthesesOverridePrecedenceAndKeepRange) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return (1 + 2) * 3; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const BinaryExpression& multiplication =
        static_cast<const BinaryExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(multiplication.OperatorType(), TokenType::Mul);
    ASSERT_EQ(multiplication.Left().Kind(),
              AstNodeKind::ParenthesizedExpression);
    const ParenthesizedExpression& parentheses =
        static_cast<const ParenthesizedExpression&>(multiplication.Left());
    EXPECT_EQ(parentheses.Inner().Kind(), AstNodeKind::BinaryExpression);
    EXPECT_EQ(parentheses.Range().Length(), 7);
}

TEST(ParserExpressionTests, ReportsMissingRightOperandWithoutThrowing) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return 1 +; }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code, DiagnosticCode::ExpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message, "expected an expression");
}

TEST(ParserAssignmentTests, AssignmentsHaveLowestPrecedence) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return result = a or b + c; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const AssignmentExpression& assignment =
        static_cast<const AssignmentExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(assignment.Target().Kind(), AstNodeKind::NameExpression);
    ASSERT_EQ(assignment.Value().Kind(), AstNodeKind::BinaryExpression);
    EXPECT_EQ(
        static_cast<const BinaryExpression&>(assignment.Value()).OperatorType(),
        TokenType::Or);
}

TEST(ParserAssignmentTests, AssignmentsAssociateRight) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return a = b = 0; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const AssignmentExpression& outer =
        static_cast<const AssignmentExpression&>(ReturnedExpression(*unit));
    EXPECT_EQ(outer.Target().Kind(), AstNodeKind::NameExpression);
    EXPECT_EQ(outer.Value().Kind(), AstNodeKind::AssignmentExpression);
}

TEST(ParserAssignmentTests, ReportsInvalidAssignmentTarget) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return (a + b) = 3; }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code,
              DiagnosticCode::InvalidAssignmentTarget);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "expression is not a valid assignment target");
}

TEST(ParserVariableDeclarationTests, ParsesTypedInitializedVariables) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("void test() { i32 x = 32; bool y = false; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    ASSERT_EQ(function.Body().Statements().size(), 2);

    const VariableDeclarationStatement& x =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    ASSERT_NE(x.ExplicitType(), nullptr);
    EXPECT_EQ(x.ExplicitType()->Type(), PrimitiveType::I32);
    EXPECT_EQ(x.Name(), "x");
    ASSERT_NE(x.Initializer(), nullptr);
    EXPECT_EQ(x.Initializer()->Kind(), AstNodeKind::LiteralExpression);

    const VariableDeclarationStatement& y =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[1]);
    ASSERT_NE(y.ExplicitType(), nullptr);
    EXPECT_EQ(y.ExplicitType()->Type(), PrimitiveType::Bool);
    EXPECT_EQ(y.Name(), "y");
    ASSERT_NE(y.Initializer(), nullptr);
    EXPECT_EQ(
        static_cast<const LiteralExpression&>(*y.Initializer()).LiteralType(),
        TokenType::False);
}

TEST(ParserVariableDeclarationTests, ParsesTypedDeclarationWithoutInitializer) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("void test() { i32 value; }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const VariableDeclarationStatement& variable =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    ASSERT_NE(variable.ExplicitType(), nullptr);
    EXPECT_EQ(variable.ExplicitType()->Type(), PrimitiveType::I32);
    EXPECT_EQ(variable.Name(), "value");
    EXPECT_EQ(variable.Initializer(), nullptr);
}

TEST(ParserVariableDeclarationTests, ParsesInferredVariables) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("void test() { x := 32; y := false; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    ASSERT_EQ(function.Body().Statements().size(), 2);

    const VariableDeclarationStatement& x =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    EXPECT_FALSE(x.HasExplicitType());
    EXPECT_EQ(x.ExplicitType(), nullptr);
    EXPECT_EQ(x.Name(), "x");
    ASSERT_NE(x.Initializer(), nullptr);
    EXPECT_EQ(x.Initializer()->Kind(), AstNodeKind::LiteralExpression);

    const VariableDeclarationStatement& y =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[1]);
    EXPECT_FALSE(y.HasExplicitType());
    EXPECT_EQ(y.Name(), "y");
    ASSERT_NE(y.Initializer(), nullptr);
    EXPECT_EQ(
        static_cast<const LiteralExpression&>(*y.Initializer()).LiteralType(),
        TokenType::False);
}

TEST(ParserVariableDeclarationTests, InferredVariablesRequireInitializer) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("void test() { value :=; }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message, "expected an expression");
}

TEST(ParserCallTests, ParsesNestedFunctionCalls) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "i32 test() { return add(1, multiply(2, 3)); }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const CallExpression& add =
        static_cast<const CallExpression&>(ReturnedExpression(*unit));
    ASSERT_EQ(add.Arguments().size(), 2);
    EXPECT_EQ(add.Callee().Kind(), AstNodeKind::NameExpression);
    EXPECT_EQ(static_cast<const NameExpression&>(add.Callee()).Name(), "add");
    ASSERT_EQ(add.Arguments()[1]->Kind(), AstNodeKind::CallExpression);
    const CallExpression& multiply =
        static_cast<const CallExpression&>(*add.Arguments()[1]);
    EXPECT_EQ(multiply.Arguments().size(), 2);
}

TEST(ParserCallTests, ParsesEmptyAndChainedCalls) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return factory()(value); }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const CallExpression& outer =
        static_cast<const CallExpression&>(ReturnedExpression(*unit));
    ASSERT_EQ(outer.Arguments().size(), 1);
    ASSERT_EQ(outer.Callee().Kind(), AstNodeKind::CallExpression);
    const CallExpression& inner =
        static_cast<const CallExpression&>(outer.Callee());
    EXPECT_TRUE(inner.Arguments().empty());
    EXPECT_EQ(outer.Range().Length(), 16);
}

TEST(ParserCallTests, CallsBindTighterThanUnaryAndBinaryOperators) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return -value(1) * other(2); }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const BinaryExpression& multiplication =
        static_cast<const BinaryExpression&>(ReturnedExpression(*unit));
    ASSERT_EQ(multiplication.Left().Kind(), AstNodeKind::UnaryExpression);
    const UnaryExpression& unary =
        static_cast<const UnaryExpression&>(multiplication.Left());
    EXPECT_EQ(unary.Operand().Kind(), AstNodeKind::CallExpression);
    EXPECT_EQ(multiplication.Right().Kind(), AstNodeKind::CallExpression);
}

TEST(ParserCallTests, ReportsMissingArgumentSeparatorAndRecovers) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return add(1 2); }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "expected ',' or ')' after argument");
}

TEST(ParserCallTests, ReportsTrailingComma) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("i32 test() { return add(1,); }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "expected an argument after ','");
}

TEST(ParserStringViewTests, ParsesMembersIndexingAndPostfixChains) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "stringv test(stringv text) { return text.substr(0, 3); } "
        "char first(stringv text) { return text.at(0); } "
        "char indexed(stringv text) { return text.substr(1, 2)[0]; }",
        diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& substringFunction =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const ReturnStatement& substringReturn =
        static_cast<const ReturnStatement&>(
            *substringFunction.Body().Statements()[0]);
    const MemberCallExpression& substring =
        static_cast<const MemberCallExpression&>(*substringReturn.Value());
    EXPECT_EQ(substring.Member(), "substr");
    EXPECT_EQ(substring.Arguments().size(), 2);

    const FunctionDeclaration& atFunction =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[1]);
    const ReturnStatement& atReturn = static_cast<const ReturnStatement&>(
        *atFunction.Body().Statements()[0]);
    EXPECT_EQ(static_cast<const MemberCallExpression&>(*atReturn.Value())
                  .Member(),
              "at");

    const FunctionDeclaration& indexFunction =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[2]);
    const ReturnStatement& indexReturn = static_cast<const ReturnStatement&>(
        *indexFunction.Body().Statements()[0]);
    const IndexExpression& index =
        static_cast<const IndexExpression&>(*indexReturn.Value());
    EXPECT_EQ(index.Receiver().Kind(), AstNodeKind::MemberCallExpression);
}

TEST(ParserConditionalTests, ParsesConditionalAlsoClauses) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "void test() { if x {} also y {} else if z {} also w {} else {} }",
        diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    ASSERT_EQ(function.Body().Statements().size(), 1);
    const IfStatement& ifStatement =
        static_cast<const IfStatement&>(*function.Body().Statements()[0]);

    ASSERT_NE(ifStatement.PrimaryBranch().Also(), nullptr);
    const AlsoClause& primaryAlso = *ifStatement.PrimaryBranch().Also();
    EXPECT_EQ(primaryAlso.Condition().Kind(), AstNodeKind::NameExpression);
    EXPECT_EQ(
        static_cast<const NameExpression&>(primaryAlso.Condition()).Name(),
        "y");

    ASSERT_EQ(ifStatement.ElseIfBranches().size(), 1);
    ASSERT_NE(ifStatement.ElseIfBranches()[0]->Also(), nullptr);
    const AlsoClause& secondaryAlso = *ifStatement.ElseIfBranches()[0]->Also();
    EXPECT_EQ(
        static_cast<const NameExpression&>(secondaryAlso.Condition()).Name(),
        "w");
    ASSERT_NE(ifStatement.ElseBody(), nullptr);
}

TEST(ParserConditionalTests, RejectsMultipleAlsoClausesOnOneBranch) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "void test() { if ready {} also warm {} also authorized {} }",
        diagnostics);

    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UnexpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "a branch may have only one 'also' clause");
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const IfStatement& ifStatement =
        static_cast<const IfStatement&>(*function.Body().Statements()[0]);
    ASSERT_NE(ifStatement.PrimaryBranch().Also(), nullptr);
    EXPECT_EQ(static_cast<const NameExpression&>(
                  ifStatement.PrimaryBranch().Also()->Condition())
                  .Name(),
              "warm");
}

TEST(ParserConditionalTests, RejectsBareAlsoClause) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit =
        ParseSource("void test() { if ready {} also {} }", diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code, DiagnosticCode::ExpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "expected a condition after 'also'");
}

TEST(ParserWhileTests, ParsesConditionalAlsoClause) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "void Test() { while running {} also ready {} }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    ASSERT_EQ(function.Body().Statements().size(), 1);
    const WhileStatement& whileStatement =
        static_cast<const WhileStatement&>(*function.Body().Statements()[0]);
    EXPECT_EQ(whileStatement.Condition().Kind(), AstNodeKind::NameExpression);
    ASSERT_NE(whileStatement.Also(), nullptr);
    EXPECT_EQ(
        static_cast<const NameExpression&>(whileStatement.Also()->Condition())
            .Name(),
        "ready");
}

TEST(ParserWhileTests, RejectsMultipleAlsoClauses) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "void Test() { while running {} also first {} also second {} }",
        diagnostics);

    ASSERT_NE(unit, nullptr);
    ASSERT_EQ(diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UnexpectedToken);
    EXPECT_EQ(diagnostics.Diagnostics()[0].message,
              "a while loop may have only one 'also' clause");
}

TEST(ParserLoopControlTests, ParsesProceedBreakJumpAndPosition) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "void Test() { while true { proceed; break; jumpto retry; "
        "#retry } }",
        diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const WhileStatement& loop =
        static_cast<const WhileStatement&>(*function.Body().Statements()[0]);
    ASSERT_EQ(loop.Body().Statements().size(), 4);
    EXPECT_EQ(loop.Body().Statements()[0]->Kind(),
              AstNodeKind::ProceedStatement);
    EXPECT_EQ(loop.Body().Statements()[1]->Kind(), AstNodeKind::BreakStatement);
    EXPECT_EQ(loop.Body().Statements()[2]->Kind(),
              AstNodeKind::JumpToStatement);
    EXPECT_EQ(loop.Body().Statements()[3]->Kind(),
              AstNodeKind::PositionStatement);
}

TEST(ParserCastTests, ParsesBothSpellingsAndChainsLeftToRight) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "i32 Test() { value := (32 as i64).cast<u8> as i32; "
        "return value; }",
        diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const VariableDeclarationStatement& declaration =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    ASSERT_EQ(declaration.Initializer()->Kind(), AstNodeKind::CastExpression);
    const CastExpression& outer =
        static_cast<const CastExpression&>(*declaration.Initializer());
    EXPECT_EQ(outer.TargetType().Type(), PrimitiveType::I32);
    ASSERT_EQ(outer.Value().Kind(), AstNodeKind::CastExpression);
    const CastExpression& middle =
        static_cast<const CastExpression&>(outer.Value());
    EXPECT_EQ(middle.TargetType().Type(), PrimitiveType::U8);
    EXPECT_EQ(middle.Value().Kind(), AstNodeKind::ParenthesizedExpression);
}

TEST(ParserCastTests, CastBindsTighterThanBinaryOperators) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "i64 Test() { return 10 + 32 as i64; }", diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    const ReturnStatement& statement = static_cast<const ReturnStatement&>(
        *function.Body().Statements()[0]);
    const BinaryExpression& binary =
        static_cast<const BinaryExpression&>(*statement.Value());
    EXPECT_EQ(binary.Right().Kind(), AstNodeKind::CastExpression);
}

TEST(ParserStringTests, ParsesOwningStringTypesAndLiteralConversions) {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit = ParseSource(
        "string Build() { string value = \"hi mom\".string(); "
        "return value.cast<string>; }",
        diagnostics);

    ASSERT_FALSE(diagnostics.HasErrors());
    const FunctionDeclaration& function =
        static_cast<const FunctionDeclaration&>(*unit->Declarations()[0]);
    EXPECT_EQ(function.ReturnType().Type(), PrimitiveType::String);
    const VariableDeclarationStatement& variable =
        static_cast<const VariableDeclarationStatement&>(
            *function.Body().Statements()[0]);
    EXPECT_EQ(variable.ExplicitType()->Type(), PrimitiveType::String);
    EXPECT_EQ(variable.Initializer()->Kind(), AstNodeKind::MemberCallExpression);
}
