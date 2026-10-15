#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/parser.hpp"
#include "core/tokenizer.hpp"
#include "semantic/scope.hpp"
#include "semantic/semantic_analyzer.hpp"

using namespace flecha::core;
using namespace flecha::diagnostics;
using namespace flecha::semantic;
using namespace flecha::source;

namespace {

struct Analysis final {
    DiagnosticBag diagnostics;
    std::unique_ptr<CompilationUnit> unit;
    SemanticModel model;
};

Analysis Analyze(const std::string& source) {
    Analysis result;
    Tokenizer tokenizer(source, result.diagnostics);
    std::vector<Token> tokens = tokenizer.Tokenize();
    Parser parser(std::move(tokens), result.diagnostics);
    result.unit = parser.Parse();
    SemanticAnalyzer analyzer(result.diagnostics);
    result.model = analyzer.Analyze(*result.unit);
    return result;
}

const Diagnostic* FindDiagnostic(const Analysis& analysis,
                                 DiagnosticCode code) {
    for (const Diagnostic& diagnostic : analysis.diagnostics.Diagnostics()) {
        if (diagnostic.code == code) return &diagnostic;
    }
    return nullptr;
}

const FunctionDeclaration& FunctionAt(const Analysis& analysis,
                                      std::size_t index) {
    return static_cast<const FunctionDeclaration&>(
        *analysis.unit->Declarations()[index]);
}

const VariableDeclarationStatement& VariableAt(
    const FunctionDeclaration& function, std::size_t index) {
    return static_cast<const VariableDeclarationStatement&>(
        *function.Body().Statements()[index]);
}

}  // namespace

TEST(SemanticTypeTests, RepresentsEverySyntaxPrimitive) {
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::I8).Name(), "i8");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::I16).Name(), "i16");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::I32).Name(), "i32");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::I64).Name(), "i64");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::ISize).Name(), "isize");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::U8).Name(), "u8");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::U16).Name(), "u16");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::U32).Name(), "u32");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::U64).Name(), "u64");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::USize).Name(), "usize");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::F32).Name(), "f32");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::F64).Name(), "f64");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::Bool).Name(), "bool");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::Char).Name(), "char");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::Void).Name(), "void");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::String).Name(),
              "string");
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::StringView).Name(),
              "stringv");
    EXPECT_TRUE(IsAssignable(SemanticType(SemanticTypeKind::I32),
                             SemanticType(SemanticTypeKind::None)));
    EXPECT_FALSE(IsAssignable(SemanticType(SemanticTypeKind::I32),
                              SemanticType(SemanticTypeKind::I64)));
}

TEST(ScopeTests, InsertsLooksUpParentsShadowsAndRejectsDuplicates) {
    const SourceRange range{{0, 1, 1}, {1, 1, 2}};
    VariableSymbol outer(SymbolKind::LocalVariable, "value",
                         SemanticType(SemanticTypeKind::I32), range, true);
    VariableSymbol inner(SymbolKind::LocalVariable, "value",
                         SemanticType(SemanticTypeKind::Bool), range, true);
    Scope global;
    Scope child(&global);

    EXPECT_TRUE(global.Declare(outer));
    EXPECT_FALSE(global.Declare(outer));
    EXPECT_EQ(child.Lookup("value"), &outer);
    EXPECT_TRUE(child.Declare(inner));
    EXPECT_EQ(child.LookupLocal("value"), &inner);
    EXPECT_EQ(child.Lookup("value"), &inner);
    EXPECT_EQ(global.Lookup("missing"), nullptr);
}

TEST(SemanticDeclarationTests, RegistersFunctionsParametersAndLocals) {
    Analysis analysis = Analyze(
        "i32 add(i32 left, i32 right) { i32 sum = left + right; "
        "return sum; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    const VariableDeclarationStatement& variable = VariableAt(function, 0);
    ASSERT_NE(analysis.model.DeclaredSymbol(function), nullptr);
    EXPECT_EQ(analysis.model.DeclaredSymbol(function)->Kind(),
              SymbolKind::Function);
    ASSERT_NE(analysis.model.DeclaredSymbol(*function.Parameters()[0]),
              nullptr);
    EXPECT_EQ(analysis.model.DeclaredSymbol(*function.Parameters()[0])->Kind(),
              SymbolKind::Parameter);
    ASSERT_NE(analysis.model.DeclaredSymbol(variable), nullptr);
    EXPECT_EQ(analysis.model.DeclaredSymbol(variable)->Type().Kind(),
              SemanticTypeKind::I32);
}

TEST(SemanticBuiltInTests, ProvidesTypedStandardOutput) {
    Analysis analysis =
        Analyze("i32 main() { stdout(\"Flecha\"); "
                "stdoutln(\"Language\"); return 42; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    const ExpressionStatement& statement =
        static_cast<const ExpressionStatement&>(
            *function.Body().Statements()[0]);
    const CallExpression& call =
        static_cast<const CallExpression&>(statement.Value());
    const NameExpression& name =
        static_cast<const NameExpression&>(call.Callee());
    const FunctionSymbol* symbol =
        static_cast<const FunctionSymbol*>(analysis.model.SymbolFor(name));
    ASSERT_NE(symbol, nullptr);
    EXPECT_EQ(symbol->BuiltIn(), BuiltInFunction::Stdout);
    EXPECT_EQ(symbol->ReturnType().Kind(), SemanticTypeKind::Void);
    ASSERT_EQ(symbol->ParameterTypes().size(), 1);
    EXPECT_EQ(symbol->ParameterTypes()[0].Kind(), SemanticTypeKind::StringView);

    const ExpressionStatement& lineStatement =
        static_cast<const ExpressionStatement&>(
            *function.Body().Statements()[1]);
    const CallExpression& lineCall =
        static_cast<const CallExpression&>(lineStatement.Value());
    const NameExpression& lineName =
        static_cast<const NameExpression&>(lineCall.Callee());
    const FunctionSymbol* lineSymbol = static_cast<const FunctionSymbol*>(
        analysis.model.SymbolFor(lineName));
    ASSERT_NE(lineSymbol, nullptr);
    EXPECT_EQ(lineSymbol->BuiltIn(), BuiltInFunction::StdoutLine);
}

TEST(SemanticBuiltInTests, ValidatesStandardOutputArguments) {
    Analysis valid = Analyze(
        "void Main() { stdout(42); stdoutln(false); stdout('x'); "
        "stdout(1.5); }");
    EXPECT_FALSE(valid.diagnostics.HasErrors());

    Analysis wrongType = Analyze("void Main() { stdout(null); }");
    EXPECT_NE(FindDiagnostic(wrongType, DiagnosticCode::IncompatibleArgument),
              nullptr);

    Analysis wrongCount = Analyze("void Main() { stdout(); }");
    EXPECT_NE(FindDiagnostic(wrongCount, DiagnosticCode::WrongArgumentCount),
              nullptr);

    Analysis lineWrongType = Analyze("void Main() { stdoutln(none); }");
    EXPECT_NE(
        FindDiagnostic(lineWrongType, DiagnosticCode::IncompatibleArgument),
        nullptr);
}

TEST(SemanticStringViewTests, TypesEveryStringViewOperation) {
    Analysis analysis = Analyze(
        "void Main() { text := \"flecha\"; length := text.len(); "
        "first := text.at(0); last := text[5 as u32]; "
        "middle := text.substr(1, 3); }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    EXPECT_EQ(analysis.model.TypeOf(*VariableAt(function, 1).Initializer())
                  .Kind(),
              SemanticTypeKind::U32);
    EXPECT_EQ(analysis.model.TypeOf(*VariableAt(function, 2).Initializer())
                  .Kind(),
              SemanticTypeKind::Char);
    EXPECT_EQ(analysis.model.TypeOf(*VariableAt(function, 3).Initializer())
                  .Kind(),
              SemanticTypeKind::Char);
    EXPECT_EQ(analysis.model.TypeOf(*VariableAt(function, 4).Initializer())
                  .Kind(),
              SemanticTypeKind::StringView);
}

TEST(SemanticStringViewTests, DiagnosesInvalidMembersIndicesAndArguments) {
    Analysis invalidMember =
        Analyze("void Main() { text := \"x\"; text.missing(); }");
    EXPECT_NE(FindDiagnostic(invalidMember, DiagnosticCode::InvalidMember),
              nullptr);

    Analysis nonStringIndex = Analyze("void Main() { value := 1; value[0]; }");
    EXPECT_NE(FindDiagnostic(nonStringIndex, DiagnosticCode::NotIndexable),
              nullptr);

    Analysis wrongIndex =
        Analyze("void Main() { text := \"x\"; text.at(true); }");
    EXPECT_NE(FindDiagnostic(wrongIndex, DiagnosticCode::IncompatibleArgument),
              nullptr);

    Analysis wrongCount =
        Analyze("void Main() { text := \"x\"; text.substr(0); }");
    EXPECT_NE(FindDiagnostic(wrongCount, DiagnosticCode::WrongArgumentCount),
              nullptr);
}

TEST(SemanticStringTests, SupportsOwningConversionsAndLiteralMembers) {
    Analysis analysis = Analyze(
        "string FromLiteral() { string first = \"hi mom\".string(); "
        "string second = \"hi mom\".cast<string>; "
        "stringv borrowed = first.view(); "
        "stringv symmetric = second.cast<stringv>; "
        "string copied = first.copy(); return copied; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(function, 0))
                  ->Type()
                  .Kind(),
              SemanticTypeKind::String);
    EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(function, 2))
                  ->Type()
                  .Kind(),
              SemanticTypeKind::StringView);
}

TEST(SemanticStringTests, FormatsScalarsOnlyAsOwningStrings) {
    Analysis valid = Analyze(
        "void Main() { integer := 42.string(); decimal := 1.5.string(); "
        "boolean := true.string(); character := 'x'.string(); "
        "castValue := 42.cast<string>; }");
    ASSERT_FALSE(valid.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(valid, 0);
    for (std::size_t index = 0; index < 5; ++index) {
        EXPECT_EQ(valid.model.DeclaredSymbol(VariableAt(function, index))
                      ->Type()
                      .Kind(),
                  SemanticTypeKind::String);
    }

    Analysis invalid = Analyze("void Main() { value := 42.cast<stringv>; }");
    EXPECT_NE(FindDiagnostic(invalid, DiagnosticCode::InvalidCast), nullptr);
}

TEST(SemanticStringTests, ConcatenatesStringsAndViewsIntoOwningStrings) {
    Analysis analysis = Analyze(
        "void Main() { string owned = \"fle\"; stringv view = \"cha\"; "
        "first := owned + view; second := view + owned; "
        "third := \"f\" + \"le\" + \"cha\"; }");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    for (std::size_t index = 2; index < 5; ++index) {
        EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(function, index))
                      ->Type()
                      .Kind(),
                  SemanticTypeKind::String);
    }
}

TEST(SemanticStringTests, RejectsViewsThatEscapeLocalOwners) {
    Analysis direct = Analyze(
        "stringv Bad() { string owned = \"temporary\"; "
        "return owned.view(); }");
    EXPECT_NE(FindDiagnostic(direct, DiagnosticCode::BorrowEscapesOwner),
              nullptr);

    Analysis indirect = Analyze(
        "stringv Bad() { string owned = \"temporary\"; "
        "stringv view = owned; return view; }");
    EXPECT_NE(FindDiagnostic(indirect, DiagnosticCode::BorrowEscapesOwner),
              nullptr);

    Analysis parameter = Analyze(
        "stringv Bad(string owned) { return owned.view(); }");
    EXPECT_NE(FindDiagnostic(parameter, DiagnosticCode::BorrowEscapesOwner),
              nullptr);

    Analysis temporary = Analyze(
        "stringv Bad() { return (\"a\" + \"b\").view(); }");
    EXPECT_NE(FindDiagnostic(temporary, DiagnosticCode::BorrowEscapesOwner),
              nullptr);

    Analysis safe = Analyze(
        "stringv Identity(stringv value) { return value; } "
        "stringv Literal() { return \"static\"; }");
    EXPECT_FALSE(safe.diagnostics.HasErrors());
}

TEST(SemanticDeclarationTests, BindsEveryNameToItsResolvedSymbol) {
    Analysis analysis =
        Analyze("i32 identity(i32 value) { copy := value; return copy; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    const VariableDeclarationStatement& variable = VariableAt(function, 0);
    const NameExpression& parameterUse =
        static_cast<const NameExpression&>(*variable.Initializer());
    const ReturnStatement& returnStatement =
        static_cast<const ReturnStatement&>(*function.Body().Statements()[1]);
    const NameExpression& localUse =
        static_cast<const NameExpression&>(*returnStatement.Value());

    EXPECT_EQ(analysis.model.SymbolFor(parameterUse),
              analysis.model.DeclaredSymbol(*function.Parameters()[0]));
    EXPECT_EQ(analysis.model.SymbolFor(localUse),
              analysis.model.DeclaredSymbol(variable));
}

TEST(SemanticDeclarationTests, DiagnosesDuplicateParameters) {
    Analysis analysis =
        Analyze("i32 choose(i32 value, i32 value) { return value; }");

    const Diagnostic* duplicate =
        FindDiagnostic(analysis, DiagnosticCode::DuplicateDeclaration);
    ASSERT_NE(duplicate, nullptr);
    EXPECT_EQ(duplicate->range.Length(), std::string("value").size());
}

TEST(SemanticDeclarationTests, SupportsExplicitInferenceAndLaterAssignments) {
    Analysis analysis = Analyze(
        "i32 main() { i32 x = 32; bool y = false; value := 32; "
        "x = 64; value = x = 7; return value; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 0))->Type().Kind(),
        SemanticTypeKind::I32);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type().Kind(),
        SemanticTypeKind::Bool);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 2))->Type().Kind(),
        SemanticTypeKind::I32);
}

TEST(SemanticInferenceTests, InfersEverySupportedLiteralCategory) {
    Analysis analysis = Analyze(
        "i32 main() { integer := 1; floating := 1.5; boolean := true; "
        "character := 'x'; text := \"hi\"; nil := null; zero := none; "
        "return integer; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    const SemanticTypeKind expected[] = {
        SemanticTypeKind::I32,    SemanticTypeKind::F64,
        SemanticTypeKind::Bool,   SemanticTypeKind::Char,
        SemanticTypeKind::StringView, SemanticTypeKind::Null,
        SemanticTypeKind::None};
    for (std::size_t index = 0; index < 7; ++index) {
        EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(function, index))
                      ->Type()
                      .Kind(),
                  expected[index]);
    }
}

TEST(SemanticInferenceTests, PropagatesTypesThroughNamesAndExpressions) {
    Analysis analysis = Analyze(
        "i32 main() { base := 32; sum := (base + 10) * 2; "
        "comparison := sum > base; return sum; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type().Kind(),
        SemanticTypeKind::I32);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 2))->Type().Kind(),
        SemanticTypeKind::Bool);
}

TEST(SemanticScopeTests, AllowsNestedShadowingAndRestoresOuterBinding) {
    Analysis analysis = Analyze(
        "i32 main() { value := 1; if true { value := false; value = true; } "
        "value = 2; return value; }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
    EXPECT_GE(analysis.model.Scopes().size(), 3);
}

TEST(SemanticScopeTests, AnalyzesWhileAndAlsoScopes) {
    Analysis analysis = Analyze(
        "i32 main() { value := 0; while value < 2 { inner := value; "
        "value = inner + 1; } also value == 1 { extra := value; "
        "value = extra + 1; } return value; }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
}

TEST(SemanticDiagnosticTests, RequiresBooleanWhileAndAlsoConditions) {
    Analysis analysis = Analyze("void Test() { while 1 {} also 2 {} }");

    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 2);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::InvalidCondition);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[1].code,
              DiagnosticCode::InvalidCondition);
}

TEST(SemanticLoopControlTests, ResolvesPositionsAcrossNestedLoops) {
    Analysis analysis = Analyze(
        "void Test() { while true { while true { jumpto outer; break; } "
        "#outer proceed; } }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
}

TEST(SemanticLoopControlTests, DiagnosesInvalidLoopControlAndPositions) {
    Analysis analysis = Analyze(
        "void Test() { proceed; break; jumpto missing; #outside "
        "while true { #duplicate #duplicate jumpto unknown; } }");

    EXPECT_NE(FindDiagnostic(analysis,
                             DiagnosticCode::LoopControlOutsideLoop),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::DuplicatePosition),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::UnknownPosition),
              nullptr);
}

TEST(SemanticCastTests, TypesChainedNumericCasts) {
    Analysis analysis = Analyze(
        "i32 main() { value := 32.cast<i64>.cast<u8> as i32; "
        "return value; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(function, 0))
                  ->Type()
                  .Kind(),
              SemanticTypeKind::I32);
}

TEST(SemanticCastTests, AllowsSameTypeCastsInBothForms) {
    Analysis analysis = Analyze(
        "i32 main() { first := 32 as i32; second := 32.cast<i32>; "
        "return first + second; }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
}

TEST(SemanticCastTests, RejectsNonNumericCasts) {
    Analysis analysis =
        Analyze("i32 main() { value := true.cast<i32>; return 0; }");

    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidCast), nullptr);
}

TEST(SemanticCastTests, CentralizesExtensibleExplicitCastPolicy) {
    EXPECT_TRUE(CanExplicitlyCast(
        SemanticType(SemanticTypeKind::I32),
        SemanticType(SemanticTypeKind::F64)));
    EXPECT_FALSE(CanExplicitlyCast(
        SemanticType(SemanticTypeKind::StringView),
        SemanticType(SemanticTypeKind::I32)));
}

TEST(SemanticStringViewTests, SupportsExplicitAndInferredViews) {
    Analysis analysis = Analyze(
        "stringv Identity(stringv value) { return value; } "
        "i32 main() { stringv explicitValue = \"Flecha\"; "
        "inferred := Identity(explicitValue); "
        "if inferred == \"Flecha\" { return 42; } return 0; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& mainFunction = FunctionAt(analysis, 1);
    EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(mainFunction, 0))
                  ->Type()
                  .Kind(),
              SemanticTypeKind::StringView);
    EXPECT_EQ(analysis.model.DeclaredSymbol(VariableAt(mainFunction, 1))
                  ->Type()
                  .Kind(),
              SemanticTypeKind::StringView);
}

TEST(SemanticDiagnosticTests, ReportsDuplicateAtExactIdentifierRange) {
    const std::string source = "i32 main() { x := 1; x := 2; return x; }";
    Analysis analysis = Analyze(source);
    const Diagnostic* diagnostic =
        FindDiagnostic(analysis, DiagnosticCode::DuplicateDeclaration);

    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(DiagnosticCodeName(diagnostic->code), "FLECHA-S0001");
    EXPECT_EQ(diagnostic->range.start.offset, source.find("x := 2"));
    EXPECT_EQ(diagnostic->range.Length(), 1);
}

TEST(SemanticDiagnosticTests,
     ReportsUnknownNameAndAssignmentBeforeDeclaration) {
    Analysis analysis =
        Analyze("i32 main() { missing; later = 1; later := 2; return later; }");

    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 2);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UnknownName);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[1].code,
              DiagnosticCode::UnknownName);
    EXPECT_EQ(DiagnosticCodeName(DiagnosticCode::UnknownName), "FLECHA-S0002");
}

TEST(SemanticDiagnosticTests, ReportsInvalidTargetAndIncompatibleAssignments) {
    Analysis analysis =
        Analyze("i32 main() { x := 1; (x + 1) = 2; x = false; return x; }");

    EXPECT_NE(FindDiagnostic(analysis,
                             DiagnosticCode::SemanticInvalidAssignmentTarget),
              nullptr);
    const Diagnostic* incompatible =
        FindDiagnostic(analysis, DiagnosticCode::IncompatibleType);
    ASSERT_NE(incompatible, nullptr);
    EXPECT_EQ(DiagnosticCodeName(incompatible->code), "FLECHA-S0003");
}

TEST(SemanticDiagnosticTests, ChecksOperatorsConditionsReturnsAndVoidLocals) {
    Analysis analysis = Analyze(
        "void noop() { return; } i32 main() { void bad = none; "
        "inferred := noop(); invalid := !1; if 1 { return false; } "
        "return false; }");

    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidVariableType),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::CannotInferType),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidOperand),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidCondition),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidReturnType),
              nullptr);
}

TEST(SemanticDiagnosticTests, RejectsAssignmentToFunctionSymbol) {
    Analysis analysis = Analyze(
        "i32 value() { return 1; } i32 main() { value = 2; return 0; }");

    const Diagnostic* diagnostic =
        FindDiagnostic(analysis, DiagnosticCode::SymbolNotWritable);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->message, "'value' is not writable");
}

TEST(SemanticCallTests, ValidatesCalleeArityAndArgumentTypes) {
    Analysis valid = Analyze(
        "i32 add(i32 a, i32 b) { return a + b; } "
        "i32 main() { result := add(1, 2); return result; }");
    EXPECT_FALSE(valid.diagnostics.HasErrors());

    Analysis invalid = Analyze(
        "i32 add(i32 a, bool b) { return a; } "
        "i32 main() { value := 1; value(); add(1); add(false, 2); "
        "return value; }");
    EXPECT_NE(FindDiagnostic(invalid, DiagnosticCode::NotCallable), nullptr);
    EXPECT_NE(FindDiagnostic(invalid, DiagnosticCode::WrongArgumentCount),
              nullptr);
    EXPECT_NE(FindDiagnostic(invalid, DiagnosticCode::IncompatibleArgument),
              nullptr);
}

TEST(SemanticCallTests, ResolvesForwardAndRecursiveCalls) {
    Analysis analysis = Analyze(
        "i32 main() { return Add(Factorial(3), 36); } "
        "i32 Add(i32 left, i32 right) { return left + right; } "
        "i32 Factorial(i32 value) { if value <= 1 { return 1; } "
        "return value * Factorial(value - 1); }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
}

TEST(SemanticCallTests, RejectsDuplicateFunctionDeclarations) {
    Analysis analysis = Analyze(
        "i32 Calculate(i32 value) { return value; } "
        "bool Calculate(bool value) { return value; }");

    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::DuplicateDeclaration),
              nullptr);
}

TEST(SemanticIntegrationTests, AnalyzesCompleteValidProgram) {
    Analysis analysis = Analyze(
        "i32 add(i32 left, i32 right) { return left + right; } "
        "i32 main() { i32 seed = 32; doubled := seed + seed; "
        "if true { doubled := 4; doubled = doubled + 1; } "
        "doubled = add(doubled, seed); return doubled; }");

    EXPECT_FALSE(analysis.diagnostics.HasErrors());
    EXPECT_TRUE(analysis.diagnostics.Empty());
}

TEST(SemanticIntegrationTests, RecoversMultipleStableExactDiagnostics) {
    const std::string source =
        "i32 id(i32 value) { return value; } "
        "i32 main() { x := 1; x := false; missing = 2; x = false; "
        "x(); id(); return false; }";
    Analysis analysis = Analyze(source);

    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::DuplicateDeclaration),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::UnknownName), nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::IncompatibleType),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::NotCallable), nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::WrongArgumentCount),
              nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::InvalidReturnType),
              nullptr);
    const Diagnostic* unknown =
        FindDiagnostic(analysis, DiagnosticCode::UnknownName);
    ASSERT_NE(unknown, nullptr);
    EXPECT_EQ(unknown->range.start.offset, source.find("missing"));
    EXPECT_EQ(unknown->range.Length(), std::string("missing").size());
}
