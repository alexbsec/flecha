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
    EXPECT_EQ(SemanticType::FromSyntax(PrimitiveType::String).Name(), "string");
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
    Analysis analysis = Analyze(
        "i32 main() { stdout(\"Flecha\"); "
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
    const FunctionSymbol* lineSymbol =
        static_cast<const FunctionSymbol*>(analysis.model.SymbolFor(lineName));
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
    EXPECT_EQ(
        analysis.model.TypeOf(*VariableAt(function, 1).Initializer()).Kind(),
        SemanticTypeKind::U32);
    EXPECT_EQ(
        analysis.model.TypeOf(*VariableAt(function, 2).Initializer()).Kind(),
        SemanticTypeKind::Char);
    EXPECT_EQ(
        analysis.model.TypeOf(*VariableAt(function, 3).Initializer()).Kind(),
        SemanticTypeKind::Char);
    EXPECT_EQ(
        analysis.model.TypeOf(*VariableAt(function, 4).Initializer()).Kind(),
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
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 0))->Type().Kind(),
        SemanticTypeKind::String);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 2))->Type().Kind(),
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

    Analysis parameter =
        Analyze("stringv Bad(string owned) { return owned.view(); }");
    EXPECT_NE(FindDiagnostic(parameter, DiagnosticCode::BorrowEscapesOwner),
              nullptr);

    Analysis temporary =
        Analyze("stringv Bad() { return (\"a\" + \"b\").view(); }");
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

TEST(SemanticDeclarationTests, SupportsDefiniteAssignmentForValueTypes) {
    Analysis valid = Analyze(
        "i32 Main(bool choose) { i32 value; string text; "
        "if choose { value = 40; text = \"left\"; } "
        "else { value = 42; text = \"right\"; } "
        "stdoutln(text); return value; }");
    EXPECT_FALSE(valid.diagnostics.HasErrors());

    Analysis directRead = Analyze("i32 Main() { i32 value; return value; }");
    EXPECT_NE(FindDiagnostic(directRead, DiagnosticCode::UninitializedVariable),
              nullptr);

    Analysis incompleteBranch = Analyze(
        "i32 Main(bool choose) { i32 value; if choose { value = 42; } "
        "return value; }");
    EXPECT_NE(
        FindDiagnostic(incompleteBranch, DiagnosticCode::UninitializedVariable),
        nullptr);

    Analysis loopOnly = Analyze(
        "i32 Main(bool run) { i32 value; while run { value = 42; break; } "
        "return value; }");
    EXPECT_NE(FindDiagnostic(loopOnly, DiagnosticCode::UninitializedVariable),
              nullptr);

    Analysis everyType = Analyze(
        "void Main() { i8 a; i16 b; i32 c; i64 d; isize e; "
        "u8 f; u16 g; u32 h; u64 i; usize j; f32 k; f64 l; "
        "bool m; char n; string o; "
        "a = 1 as i8; b = 1 as i16; c = 1; d = 1 as i64; "
        "e = 1 as isize; f = 1 as u8; g = 1 as u16; h = 1 as u32; "
        "i = 1 as u64; j = 1 as usize; k = 1.0 as f32; l = 1.0; "
        "m = true; n = 'x'; o = \"ready\"; stdoutln(o); }");
    EXPECT_FALSE(everyType.diagnostics.HasErrors());
}

TEST(SemanticDeclarationTests, BorrowedViewsRequireImmediateInitialization) {
    Analysis analysis = Analyze("void Main() { stringv view; }");
    EXPECT_NE(
        FindDiagnostic(analysis, DiagnosticCode::BorrowRequiresInitializer),
        nullptr);
}

TEST(SemanticInferenceTests, InfersEverySupportedLiteralCategory) {
    Analysis analysis = Analyze(
        "i32 main() { integer := 1; floating := 1.5; boolean := true; "
        "character := 'x'; text := \"hi\"; nil := null; zero := none; "
        "return integer; }");

    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 0);
    const SemanticTypeKind expected[] = {
        SemanticTypeKind::I32,        SemanticTypeKind::F64,
        SemanticTypeKind::Bool,       SemanticTypeKind::Char,
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

    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::LoopControlOutsideLoop),
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
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 0))->Type().Kind(),
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
    EXPECT_TRUE(CanExplicitlyCast(SemanticType(SemanticTypeKind::I32),
                                  SemanticType(SemanticTypeKind::F64)));
    EXPECT_FALSE(CanExplicitlyCast(SemanticType(SemanticTypeKind::StringView),
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

TEST(SemanticControlFlowAssignmentTests, IntersectsOnlyFallthroughBranches) {
    const std::vector<std::string> programs = {
        "i32 main(bool c) { i32 x; if c { return 0; } else { x = 42; } return "
        "x; }",
        "i32 main(bool c) { i32 x; if c { x = 42; } else { return 0; } return "
        "x; }",
        "i32 main(bool a, bool b) { i32 x; if a { return 0; } "
        "else if b { x = 42; } else { x = 40; } return x; }",
        "i32 main(bool a, bool b) { i32 x; if a { if b { x = 42; } "
        "else { return 0; } } else { x = 40; } return x; }",
        "i32 main(bool c) { string x; if c { x = \"yes\"; } "
        "else { x = \"no\"; } stdoutln(x); return 42; }",
        "i32 main(bool c) { i32 x; if c { return 1; } else { return 2; } "
        "return x; }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        EXPECT_FALSE(Analyze(program).diagnostics.HasErrors());
    }
}

TEST(SemanticControlFlowAssignmentTests, RejectsIncompleteBranchJoins) {
    const std::vector<std::string> programs = {
        "i32 main(bool c) { i32 x; if c { x = 42; } else {} return x; }",
        "i32 main(bool c) { i32 x; if c { x = 42; } return x; }",
        "i32 main(bool a, bool b) { i32 x; if a { x = 42; } "
        "else if b { return 0; } return x; }",
        "i32 main(bool a, bool b) { i32 x; if a { if b { x = 42; } } "
        "else { x = 40; } return x; }",
        "i32 main(bool c) { string x; if c { x = \"yes\"; } stdoutln(x); "
        "return 0; }",
        "i32 main(bool c) { i32 x; if c {} also c { x = 42; } return x; }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        Analysis analysis = Analyze(program);
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::UninitializedVariable);
    }
}

TEST(SemanticControlFlowAssignmentTests, ModelsBreakProceedAndAlsoEdges) {
    const std::vector<std::string> programs = {
        "void main(bool c) { while c { i32 x; if c { break; } "
        "else { x = 42; } stdoutln(x); } }",
        "void main(bool c) { while c { i32 x; if c { proceed; } "
        "else { x = 42; } stdoutln(x); } }",
        "void main(bool c) { i32 x; while c { if c { proceed; } "
        "else { x = 42; } } also c { stdoutln(x); } }",
        "void main(bool c) { i32 x; while c { if c { break; } "
        "else { x = 42; } } also c { stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { x = 42; } "
        "also c { stdoutln(x); proceed; } }",
        "void main(bool c) { while c { i32 x; while c { break; } "
        "x = 42; stdoutln(x); } }",
        "void main(bool c) { while c { i32 x; proceed; stdoutln(x); } }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        EXPECT_FALSE(Analyze(program).diagnostics.HasErrors());
    }
}

TEST(SemanticControlFlowAssignmentTests,
     RejectsLoopOnlyAndUnreachableAssignments) {
    const std::vector<std::string> programs = {
        "i32 main(bool c) { i32 x; while c { x = 42; break; } return x; }",
        "i32 main(bool c) { i32 x; while c { x = 42; proceed; } return x; }",
        "i32 main(bool c) { i32 x; while c { proceed; x = 42; } return x; }",
        "void main(bool c) { i32 x; while c { stdoutln(x); x = 42; proceed; } "
        "}",
        "void main(bool c) { while c { i32 x; stdoutln(x); x = 42; } }",
        "void main(bool c) { i32 x; while c { while c { x = 42; break; } "
        "stdoutln(x); } }",
        "void main(bool c) { i32 x; while c {} also c { x = 42; } stdoutln(x); "
        "}",
        "void main(bool c) { i32 x; while c {} also c { stdoutln(x); } }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        Analysis analysis = Analyze(program);
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::UninitializedVariable);
    }
}

TEST(SemanticControlFlowAssignmentTests, FollowsForwardBackwardAndNestedJumps) {
    const std::vector<std::string> programs = {
        "void main(bool c) { i32 x; while c { x = 42; jumpto end; "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { jumpto init; "
        "#read stdoutln(x); break; #init x = 42; jumpto read; } }",
        "void main(bool c) { i32 x; while c { if c { x = 1; jumpto end; } "
        "else { x = 2; jumpto end; } #end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { if c { x = 1; jumpto end; } "
        "x = 2; #end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { jumpto inner; "
        "#outer stdoutln(x); break; while c { #inner x = 42; jumpto outer; } } "
        "}",
        "void main(bool c) { while c { i32 x; if c { jumpto end; } "
        "else { x = 42; } stdoutln(x); #end break; } }",
        "void main(bool c) { while c { jumpto position; string x; "
        "#position x = \"ready\"; stdoutln(x); break; } }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        EXPECT_FALSE(Analyze(program).diagnostics.HasErrors());
    }
}

TEST(SemanticControlFlowAssignmentTests,
     RejectsBypassedDeclarationsAndAssignments) {
    const std::vector<std::string> programs = {
        "void main(bool c) { i32 x; while c { jumpto end; x = 42; "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { while c { jumpto end; x := 42; "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { while c { jumpto end; string x = \"ready\"; "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { while c { jumpto end; stringv x = \"ready\"; "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { if c { jumpto end; } "
        "x = 42; #end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { if c { x = 42; jumpto end; } "
        "#end stdoutln(x); break; } }",
        "void main(bool c) { i32 x; while c { #read stdoutln(x); "
        "x = 42; jumpto read; } }",
        "void main(bool c) { i32 x; while c { jumpto inner; "
        "#outer stdoutln(x); break; while c { #inner jumpto outer; x = 42; } } "
        "}",
        "void main(bool c) { while c { jumpto inner; while c { x := 42; "
        "#inner stdoutln(x); break; } } }",
        "void main(bool c) { while c { #again i32 x; if c { x = 42; jumpto "
        "again; } "
        "stdoutln(x); break; } }",
    };
    for (const std::string& program : programs) {
        SCOPED_TRACE(program);
        Analysis analysis = Analyze(program);
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::UninitializedVariable);
    }
}

TEST(SemanticControlFlowAssignmentTests, DropsLocalsWhenJumpsLeaveTheirScope) {
    Analysis analysis = Analyze(
        "void main(bool c) { while c { if c { string x = \"owned\"; "
        "jumpto outside; #inside stdoutln(x); break; } "
        "#outside jumpto inside; } }");
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UninitializedVariable);
}

TEST(SemanticControlFlowAssignmentTests, DistinguishesShadowedSymbols) {
    Analysis invalid = Analyze(
        "i32 main(bool c) { i32 x; if c { x := 42; stdoutln(x); } "
        "else { x = 40; } return x; }");
    ASSERT_EQ(invalid.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(invalid.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UninitializedVariable);
    Analysis valid = Analyze(
        "i32 main(bool c) { x := 42; if c { i32 x; x = 1; stdoutln(x); } "
        "return x; }");
    EXPECT_FALSE(valid.diagnostics.HasErrors());
    Analysis branchLocal =
        Analyze("i32 main(bool c) { if c { x := 42; } return x; }");
    EXPECT_NE(FindDiagnostic(branchLocal, DiagnosticCode::UnknownName),
              nullptr);
}

TEST(SemanticControlFlowAssignmentTests,
     HonorsExpressionEvaluationAndShortCircuit) {
    const std::vector<std::string> valid = {
        "i32 main(bool c) { i32 x; if (x = 42) == 42 { } return x; }",
        "void main(bool c) { bool x; if c and (x = c) { stdoutln(x); } }",
        "void main(bool c) { bool x; if c or (x = c) {} else { stdoutln(x); } "
        "}",
        "void main(bool c) { bool x; if !(c and (x = c)) {} else { "
        "stdoutln(x); } }",
        "i32 main(bool c) { i32 x; while (x = 42) == 0 {} return x; }",
        "i32 main() { i32 x; return (x = 21) + x; }",
    };
    for (const std::string& program : valid) {
        SCOPED_TRACE(program);
        EXPECT_FALSE(Analyze(program).diagnostics.HasErrors());
    }
    const std::vector<std::string> invalid = {
        "bool main(bool c) { bool x; c and (x = true); return x; }",
        "bool main(bool c) { bool x; c or (x = true); return x; }",
        "void main(bool c) { bool x; if c and (x = c) {} else { stdoutln(x); } "
        "}",
        "void main(bool c) { bool x; if c or (x = c) { stdoutln(x); } }",
        "i32 main() { i32 x; return x + (x = 21); }",
        "i32 main() { i32 x; x = x + 1; return x; }",
    };
    for (const std::string& program : invalid) {
        SCOPED_TRACE(program);
        Analysis analysis = Analyze(program);
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::UninitializedVariable);
    }
}

TEST(SemanticControlFlowAssignmentTests, ReportsExactReachableReadRangeOnce) {
    const std::string source =
        "void main(bool c) {\n"
        "    i32 value;\n"
        "    while c {\n"
        "        jumpto read;\n"
        "        value = 42;\n"
        "        #read stdoutln(value);\n"
        "        jumpto read;\n"
        "    }\n"
        "}";
    Analysis analysis = Analyze(source);
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
    const Diagnostic& diagnostic = analysis.diagnostics.Diagnostics()[0];
    EXPECT_EQ(DiagnosticCodeName(diagnostic.code), "FLECHA-S0021");
    EXPECT_EQ(diagnostic.range.start.offset, source.find("value);"));
    EXPECT_EQ(diagnostic.range.start.line, 6);
    EXPECT_EQ(diagnostic.range.start.column, 24);
    EXPECT_EQ(diagnostic.range.end.line, 6);
    EXPECT_EQ(diagnostic.range.end.column, 29);
    EXPECT_EQ(diagnostic.range.Length(), 5);
}

TEST(SemanticTypeTests, NormalizesAliasesAndRetainsEachQualifierLayer) {
    const std::vector<std::pair<std::string, std::string>> aliases{
        {"constptr u8", "const *u8"},
        {"constref u8", "const &u8"},
        {"constref &u8", "const & &u8"},
        {"&constptr *constref const u8", "&const **const &const u8"}};
    for (const auto& [alias, expanded] : aliases) {
        SCOPED_TRACE(alias);
        Analysis analysis =
            Analyze("void f(" + alias + " a, " + expanded + " b) {}");
        const FunctionDeclaration& function = FunctionAt(analysis, 0);
        const SemanticType first =
            SemanticType::FromSyntax(function.Parameters()[0]->Type());
        const SemanticType second =
            SemanticType::FromSyntax(function.Parameters()[1]->Type());
        EXPECT_EQ(first, second);
        EXPECT_EQ(first.Name(), expanded);
        const FunctionSymbol& symbol = static_cast<const FunctionSymbol&>(
            *analysis.model.DeclaredSymbol(function));
        EXPECT_EQ(symbol.ParameterTypes()[0], first);
        EXPECT_EQ(symbol.ParameterTypes()[1], second);
    }
    const SemanticType byte(SemanticTypeKind::U8);
    const SemanticType readOnlyByte(SemanticTypeKind::U8, true);
    const SemanticType fixedPointer(SemanticTypeKind::Pointer, byte, true);
    const SemanticType readOnlyPointer(SemanticTypeKind::Pointer, readOnlyByte);
    EXPECT_NE(fixedPointer, readOnlyPointer);
    EXPECT_FALSE(IsAssignable(fixedPointer, readOnlyPointer));
    EXPECT_TRUE(IsAssignable(readOnlyPointer, fixedPointer));
    EXPECT_EQ(fixedPointer.WithoutConst().Target()->Name(), "u8");
    EXPECT_EQ(readOnlyPointer.WithoutConst().Target()->Name(), "const u8");
    const SemanticType reference(SemanticTypeKind::Reference, byte);
    EXPECT_FALSE(IsAssignable(reference, SemanticType(SemanticTypeKind::None)));
    EXPECT_FALSE(CanExplicitlyCast(reference, reference));
    EXPECT_FALSE(CanExplicitlyCast(fixedPointer, readOnlyPointer));
    EXPECT_TRUE(IsAssignable(readOnlyByte, byte));
}

TEST(SemanticConstTests, AllowsRuntimeInitializationAndMutableValueCopies) {
    Analysis analysis = Analyze(
        "const i32 get(const i32 value) { return value; } "
        "i32 main() { const i32 answer = get(42); copy := answer; "
        "copy = copy + 1; const i32 zero = none; "
        "const bool yes = answer == 42; const i64 wide = answer as const i64; "
        "const string text = \"hi\"; const stringv view = text.view(); "
        "return copy - 1; }");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const FunctionDeclaration& function = FunctionAt(analysis, 1);
    const Symbol* answer =
        analysis.model.DeclaredSymbol(VariableAt(function, 0));
    const Symbol* copy = analysis.model.DeclaredSymbol(VariableAt(function, 1));
    ASSERT_NE(answer, nullptr);
    ASSERT_NE(copy, nullptr);
    EXPECT_FALSE(answer->Writable());
    EXPECT_TRUE(answer->Type().IsConst());
    EXPECT_TRUE(copy->Writable());
    EXPECT_FALSE(copy->Type().IsConst());
    EXPECT_FALSE(analysis.model.TypeOf(*VariableAt(function, 1).Initializer())
                     .IsConst());
}

TEST(SemanticConstTests, RejectsWritesToLocalsAndParametersAtExactNameRange) {
    const std::vector<std::string> sources{
        "void f() {\n    const i32 value = 1;\n    value = 2;\n}",
        "void f(const i32 value) {\n    value = 2;\n}"};
    for (const std::string& source : sources) {
        SCOPED_TRACE(source);
        Analysis analysis = Analyze(source);
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        const Diagnostic& diagnostic = analysis.diagnostics.Diagnostics()[0];
        EXPECT_EQ(DiagnosticCodeName(diagnostic.code), "FLECHA-S0005");
        EXPECT_EQ(diagnostic.range.start.offset, source.rfind("value"));
        EXPECT_EQ(diagnostic.range.Length(), 5);
        EXPECT_EQ(diagnostic.range.start.column, 5);
        EXPECT_EQ(diagnostic.range.end.column, 10);
    }
}

TEST(SemanticConstTests, RequiresInitializerAndPreservesBorrowRestrictions) {
    Analysis analysis = Analyze("void f() { const i32 value; }");
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
    const Diagnostic& diagnostic = analysis.diagnostics.Diagnostics()[0];
    EXPECT_EQ(DiagnosticCodeName(diagnostic.code), "FLECHA-S0023");
    EXPECT_EQ(diagnostic.range.start.offset, 21);
    EXPECT_EQ(diagnostic.range.Length(), 5);
    Analysis borrowed = Analyze("void f() { const stringv value; }");
    EXPECT_NE(
        FindDiagnostic(borrowed, DiagnosticCode::BorrowRequiresInitializer),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(borrowed, DiagnosticCode::ConstRequiresInitializer),
        nullptr);
    Analysis escaped = Analyze(
        "stringv f() { const string value = \"hi\"; return value.view(); }");
    EXPECT_NE(FindDiagnostic(escaped, DiagnosticCode::BorrowEscapesOwner),
              nullptr);
    Analysis invalid = Analyze("void f() { const void value = none; }");
    EXPECT_NE(FindDiagnostic(invalid, DiagnosticCode::InvalidVariableType),
              nullptr);
}

TEST(SemanticConstTests,
     UsesResolvedSymbolsForShadowingAndConstvalIsAnIdentifier) {
    Analysis analysis = Analyze(
        "i32 f() { const i32 value = 42; if true { i32 value = 0; "
        "value = 1; } constval := value; constval = constval + 1; "
        "return constval - 1; }");
    EXPECT_FALSE(analysis.diagnostics.HasErrors());
    Analysis invalid = Analyze(
        "i32 f() { i32 value = 42; if true { const i32 value = 0; "
        "value = 1; } value = 43; return value - 1; }");
    ASSERT_EQ(invalid.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(invalid.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::SymbolNotWritable);
}

TEST(SemanticConstTests, CannotBypassInitializationWithJump) {
    Analysis analysis = Analyze(
        "i32 f() { while true { jumpto end; const i32 value = 42; #end return "
        "value; } return 0; }");
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UninitializedVariable);
}

TEST(SemanticCompoundTypeTests, RejectsUnsupportedNestedReferenceStorage) {
    for (const std::string& type :
         {"*&i32", "& &i32", "&string", "&stringv", "&void"}) {
        const std::string source = "void f(" + type + " value) {}";
        SCOPED_TRACE(source);
        Analysis analysis = Analyze(source);
        const Diagnostic* diagnostic =
            FindDiagnostic(analysis, DiagnosticCode::UnsupportedCompoundType);
        ASSERT_NE(diagnostic, nullptr);
        EXPECT_EQ(diagnostic->range.start.offset, source.find(type));
        EXPECT_EQ(diagnostic->range.Length(), type.size());
    }
}

TEST(SemanticCompoundTypeTests, RejectsDereferencingScalar) {
    EXPECT_NE(
        FindDiagnostic(Analyze("i32 f() { i32 value = 42; return *value; }"),
                       DiagnosticCode::InvalidOperand),
        nullptr);
}

TEST(SemanticUnsafeTests, EnforcesLexicalPermissionAndExactRanges) {
    const std::string source =
        "unsafe void touch(*i32 p) { *p = 42; }\n"
        "void f(*i32 p) {\n"
        "    touch(p);\n"
        "    *p;\n"
        "    *p = 1;\n"
        "    &raw p;\n"
        "    unsafe { touch(p); { *p = 2; } }\n"
        "    touch(p);\n"
        "}";
    Analysis analysis = Analyze(source);
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 5);
    const std::vector<std::size_t> lines{3, 4, 5, 6, 8};
    const std::vector<std::size_t> lengths{5, 2, 2, 6, 5};
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const Diagnostic& diagnostic =
            analysis.diagnostics.Diagnostics()[index];
        EXPECT_EQ(DiagnosticCodeName(diagnostic.code), "FLECHA-S0025");
        EXPECT_EQ(diagnostic.range.start.line, lines[index]);
        EXPECT_EQ(diagnostic.range.start.column, 5);
        EXPECT_EQ(diagnostic.range.Length(), lengths[index]);
    }
    Analysis callee = Analyze(
        "void safe(*i32 p) { *p = 1; } unsafe void caller(*i32 p) { safe(p); "
        "}");
    ASSERT_EQ(callee.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(callee.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::UnsafeOperation);
}

TEST(SemanticUnsafeTests, AllowsUnsafeCallsAndPointerTransportInSafeCode) {
    Analysis analysis = Analyze(
        "unsafe i32 read(*const i32 p) { return *p; } "
        "unsafe i32 forward(*i32 p) { return read(p); } "
        "*i32 transport(*i32 p) { *i32 copy = p; return copy; } "
        "i32 main() { i32 value = 42; *i32 p = none; "
        "unsafe { p = &raw value; } p = transport(p); "
        "if p != null and none != p { unsafe { return forward(p); } } return "
        "0; }");
    EXPECT_FALSE(analysis.diagnostics.HasErrors());
}

TEST(SemanticUnsafeTests, SeparatesBindingConstnessFromTargetConstness) {
    Analysis valid = Analyze(
        "unsafe void f() { i32 x = 1; const i32 y = 2; "
        "constptr i32 fixed = &raw x; *fixed = 3; "
        "*const i32 readOnly = &raw y; readOnly = &raw x; "
        "constptr const i32 both = &raw y; i32 copy = *both; "
        "**i32 nested = &raw readOnly; }");
    // A pointer to a read-only pointer target cannot become **i32.
    ASSERT_EQ(valid.diagnostics.Diagnostics().size(), 1);
    EXPECT_EQ(valid.diagnostics.Diagnostics()[0].code,
              DiagnosticCode::IncompatibleType);
    const std::vector<std::string> invalid{
        "constptr i32 fixed = none; fixed = none;",
        "*const i32 p = none; *p = 1;", "constptr const i32 p = none; *p = 1;",
        "const i32 x = 1; *(&raw x) = 2;", "*const *i32 p = none; *p = none;"};
    for (const std::string& body : invalid) {
        SCOPED_TRACE(body);
        Analysis analysis = Analyze("unsafe void f() { " + body + " }");
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::SymbolNotWritable);
    }
    Analysis deep = Analyze("unsafe void f(**i32 p) { **const i32 q = p; }");
    EXPECT_NE(FindDiagnostic(deep, DiagnosticCode::IncompatibleType), nullptr);
    Analysis missing = Analyze("void f() { constptr i32 p; }");
    EXPECT_NE(FindDiagnostic(missing, DiagnosticCode::ConstRequiresInitializer),
              nullptr);
}

TEST(SemanticUnsafeTests, ChecksInitializedAddressesAndIndirectStoreTargets) {
    const std::vector<std::string> invalid{
        "unsafe void f() { i32 x; *i32 p = &raw x; }",
        "unsafe void f() { *i32 p; *p = 42; }",
        "unsafe void f() { *i32 p; *p = 42; *p; }",
        "i32 init(*i32 p) { return 42; } unsafe void f() { *i32 p; *p = init(p "
        "= none); }",
        "void f() { i32 x; unsafe { i32 y = x; } }",
        "void f(bool c) { i32 x; if c { unsafe { x = 1; } } stdoutln(x); }",
        "void f() { while true { jumpto use; unsafe { *i32 p = none; #use *p = "
        "1; } } }"};
    for (const std::string& source : invalid) {
        SCOPED_TRACE(source);
        Analysis analysis = Analyze(source);
        EXPECT_NE(
            FindDiagnostic(analysis, DiagnosticCode::UninitializedVariable),
            nullptr);
    }
    Analysis valid = Analyze(
        "i32 f() { i32 x; unsafe { (x) = 42; } { i32 x = 0; x = 1; } return x; "
        "}");
    EXPECT_FALSE(valid.diagnostics.HasErrors());
    Analysis scope =
        Analyze("void f() { unsafe { i32 x = 42; } stdoutln(x); }");
    EXPECT_NE(FindDiagnostic(scope, DiagnosticCode::UnknownName), nullptr);
}

TEST(SemanticUnsafeTests,
     RejectsUnsupportedAddressTargetsAndPointerOperations) {
    const std::vector<std::string> invalid{
        "unsafe void f() { &raw 42; }", "unsafe void f() { &raw (1 + 2); }",
        "unsafe void f() { &raw f; }", "unsafe void f(*void p) { *p; }"};
    for (const std::string& source : invalid) {
        SCOPED_TRACE(source);
        EXPECT_NE(FindDiagnostic(Analyze(source),
                                 DiagnosticCode::InvalidPointerAccess),
                  nullptr);
    }
    for (const std::string& body : {"p + 1;", "p - p;", "p < p;", "p as i64;",
                                    "1 as *i32;", "p as *const i32;"}) {
        SCOPED_TRACE(body);
        EXPECT_TRUE(Analyze("unsafe void f(*i32 p) { " + body + " }")
                        .diagnostics.HasErrors());
    }
    EXPECT_NE(FindDiagnostic(Analyze("void f(*&i32 p) {}"),
                             DiagnosticCode::UnsupportedCompoundType),
              nullptr);
    EXPECT_FALSE(
        Analyze("unsafe void f() { i32 x = 1; &x; }").diagnostics.HasErrors());
}

TEST(SemanticUnsafeTests, RejectsConstTargetWritesAtTheDereferenceRange) {
    const std::string source = "unsafe void f(*const i32 p) {\n    *p = 42;\n}";
    Analysis analysis = Analyze(source);
    ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
    const Diagnostic& diagnostic = analysis.diagnostics.Diagnostics()[0];
    EXPECT_EQ(diagnostic.code, DiagnosticCode::SymbolNotWritable);
    EXPECT_EQ(diagnostic.range.start.line, 2);
    EXPECT_EQ(diagnostic.range.start.column, 5);
    EXPECT_EQ(diagnostic.range.end.column, 7);
}

TEST(SemanticUnsafeTests,
     AddressCreationPreservesNestedConstAndRawIsContextual) {
    Analysis analysis = Analyze(
        "unsafe void f() { const i32 value = 42; p := &raw value; "
        "constptr i32 fixed = none; slot := &raw fixed; raw := 1; raw = 2; }");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const auto& function = FunctionAt(analysis, 0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type().Name(),
        "*const i32");
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 3))->Type().Name(),
        "*const *i32");
    Analysis deep = Analyze("unsafe void f(*const *i32 slot) { **slot = 42; }");
    EXPECT_FALSE(deep.diagnostics.HasErrors());
}

TEST(SemanticUnsafeTests,
     RejectsIncompatibleNullComparisonsBeforeCodeGeneration) {
    for (const std::string& expression :
         {"null == 1", "null === 1", "1 !== null", "p == 1", "p === none"}) {
        SCOPED_TRACE(expression);
        Analysis analysis =
            Analyze("bool f(*i32 p) { return " + expression + "; }");
        ASSERT_EQ(analysis.diagnostics.Diagnostics().size(), 1);
        EXPECT_EQ(analysis.diagnostics.Diagnostics()[0].code,
                  DiagnosticCode::InvalidOperand);
    }
}

TEST(SemanticReferenceTests, InfersReferenceAndContextualByteLiterals) {
    Analysis analysis = Analyze(
        "u8 f() { u8 value = 3; ref := &value; *ref = *ref * 2; return value; "
        "}");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const auto& function = FunctionAt(analysis, 0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type().Name(),
        "&u8");
    EXPECT_EQ(
        analysis.model.TypeOf(*VariableAt(function, 0).Initializer()).Name(),
        "u8");
}

TEST(SemanticReferenceTests, CopiesSharedHandlesAndInfersConstStorage) {
    Analysis analysis = Analyze(
        "i32 f() { const i32 value = 3; r := &value; copy := r; return *r + "
        "*copy; }"
        "i32 g() { i32 value = 3; &const i32 r = &value; copy := r; return *r "
        "+ *copy; }");
    EXPECT_FALSE(analysis.diagnostics.HasErrors());
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(FunctionAt(analysis, 0), 1))
            ->Type()
            .Name(),
        "&const i32");
}

TEST(SemanticReferenceTests,
     TransfersMutableHandlesAndReinitializesMovedBindings) {
    for (const std::string& body :
         {"r := &x; moved := r; *moved = 4; return x;",
          "r := &x; moved := r; *moved = 4; r = &y; *r = 5; return x + y;",
          "r := &x; r = &y; *r = 5; return x + y;",
          "r := &x; r = r; *r = 5; return x;"}) {
        SCOPED_TRACE(body);
        EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; i32 y = 2; " + body + " }")
                         .diagnostics.HasErrors());
    }
    const std::string source =
        "i32 f() { i32 x = 1; r := &x; moved := r; return *r; }";
    Analysis analysis = Analyze(source);
    const Diagnostic* diagnostic =
        FindDiagnostic(analysis, DiagnosticCode::MovedReference);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->range.start.offset, source.rfind("r;"));
    EXPECT_EQ(diagnostic->range.Length(), 1);
}

TEST(SemanticReferenceTests,
     FixedBindingsAllowMutationAndReborrowingButNotMoves) {
    EXPECT_FALSE(Analyze("void use(&i32 p) { *p = 6; }"
                         "i32 f() { i32 x = 1; constref i32 r = &x; *r = 2; "
                         "use(r); return x; }")
                     .diagnostics.HasErrors());
    EXPECT_NE(
        FindDiagnostic(
            Analyze("void f() { i32 x = 1; constref i32 r = &x; moved := r; }"),
            DiagnosticCode::MovedReference),
        nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("void f() { i32 x = 1; i32 y = 2; "
                                     "constref i32 r = &x; r = &y; }"),
                             DiagnosticCode::SymbolNotWritable),
              nullptr);
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; constref const i32 r = &x; copy "
                         ":= r; return *copy + *r; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests,
     RequiresInitializedNonNullStorageAndCompatibleTypes) {
    const std::vector<std::pair<std::string, DiagnosticCode>> cases{
        {"&i32 r;", DiagnosticCode::ReferenceRequiresInitializer},
        {"&i32 r = none;", DiagnosticCode::IncompatibleType},
        {"&i32 r = null;", DiagnosticCode::IncompatibleType},
        {"i32 x; r := &x;", DiagnosticCode::UninitializedVariable},
        {"r := &42;", DiagnosticCode::InvalidReference},
        {"i32 x = 1; u32 y = 2; r := &x; r = &y;",
         DiagnosticCode::IncompatibleType},
        {"i32 x = 1; r := &x; r = 2;", DiagnosticCode::IncompatibleType},
        {"const i32 x = 1; &i32 r = &x;", DiagnosticCode::IncompatibleType},
        {"i32 x = 1; &const i32 r = &x; *r = 2;",
         DiagnosticCode::SymbolNotWritable},
        {"i32 x = 1; r := &x; r as &i32;", DiagnosticCode::InvalidCast},
        {"i32 x = 1; r := &x; r == r;", DiagnosticCode::InvalidOperand},
    };
    for (const auto& [body, code] : cases) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + body + " }"), code),
                  nullptr);
    }
}

TEST(SemanticReferenceTests, ChecksConflictsUntilLastUse) {
    for (const std::string& body :
         {"r := &x; x = 2; return *r;",
          "r := &x; i32 copy = x; return *r + copy;",
          "r := &x; other := &x; return *r + *other;",
          "&const i32 r = &x; other := &x; return *r + *other;",
          "&const i32 r = &x; x = 2; return *r;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("i32 f() { i32 x = 1; " + body + " }"),
                                 DiagnosticCode::BorrowConflict),
                  nullptr);
    }
    EXPECT_FALSE(
        Analyze("i32 f() { i32 x = 1; r := &x; *r = 4; x = x + 2; return x; }")
            .diagnostics.HasErrors());
    EXPECT_FALSE(
        Analyze("i32 f() { i32 x = 1; &const i32 r = &x; return x + *r; }")
            .diagnostics.HasErrors());
    const std::string source =
        "i32 f() { i32 x = 1; r := &x; x = 2; return *r; }";
    Analysis analysis = Analyze(source);
    const Diagnostic* diagnostic =
        FindDiagnostic(analysis, DiagnosticCode::BorrowConflict);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->range.start.offset, source.find("x = 2"));
    EXPECT_EQ(diagnostic->range.Length(), 1);
}

TEST(SemanticReferenceTests, ReborrowsForCallsWithoutMovingTheCallerHandle) {
    EXPECT_FALSE(Analyze("void bump(&i32 r) { *r = *r + 1; }"
                         "i32 read(&const i32 r) { return *r; }"
                         "i32 f() { i32 x = 1; r := &x; bump(r); bump(r); "
                         "return read(r) + *r; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(
        Analyze("i32 sum(&const i32 a, &const i32 b) { return *a + *b; }"
                "i32 f() { i32 x = 3; r := &x; return sum(r, r); }")
            .diagnostics.HasErrors());
    for (const std::string& arguments :
         {"r, r", "&*r, r", "r, &*r", "&x, &x"}) {
        SCOPED_TRACE(arguments);
        EXPECT_NE(FindDiagnostic(Analyze("void use(&i32 a, &i32 b) {} void f() "
                                         "{ i32 x = 1; r := &x; use(" +
                                         arguments + "); }"),
                                 DiagnosticCode::BorrowConflict),
                  nullptr);
    }
    EXPECT_NE(FindDiagnostic(Analyze("void use(&i32 a, i32 b) {} void f() { "
                                     "i32 x = 1; use(&x, x); }"),
                             DiagnosticCode::BorrowConflict),
              nullptr);
    EXPECT_FALSE(
        Analyze(
            "void use(i32 a, &i32 b) {} void f() { i32 x = 1; use(x, &x); }")
            .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests, ChildReborrowsFreezeParentsUntilLastUse) {
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; r := &x; child := &*r; *child = "
                         "3; *r = 6; return x; }")
                     .diagnostics.HasErrors());
    for (const std::string& middle :
         {"*r = 3;", "i32 copy = *r;", "r = &y;", "moved := r;"}) {
        SCOPED_TRACE(middle);
        EXPECT_NE(
            FindDiagnostic(
                Analyze(
                    "i32 f() { i32 x = 1; i32 y = 2; r := &x; child := &*r; " +
                    middle + " return *child; }"),
                DiagnosticCode::BorrowConflict),
            nullptr);
    }
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; &const i32 r = &x; child := "
                         "&*r; return *r + *child; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests, RejectsEscapesAndUnsupportedConversions) {
    for (const std::string& source :
         {"&i32 f(&i32 r) { return r; }",
          "void f() { i32 x = 1; r := &x; { i32 y = 2; r = &y; } }",
          "void f() { i32 x = 1; r := &x; { child := &x; r = &*child; } }",
          "unsafe void f() { i32 x = 1; r := &x; &raw r; }",
          "unsafe void f() { i32 x = 1; r := &x; &raw *r; }"}) {
        SCOPED_TRACE(source);
        EXPECT_NE(
            FindDiagnostic(Analyze(source), DiagnosticCode::ReferenceEscape),
            nullptr);
    }
    EXPECT_NE(FindDiagnostic(Analyze("unsafe void f(*i32 p) { r := &*p; }"),
                             DiagnosticCode::InvalidReference),
              nullptr);
}

TEST(SemanticReferenceTests,
     ReferencesToPointerSlotsStillRequireUnsafePointeeAccess) {
    EXPECT_FALSE(Analyze("void f(*i32 p, *i32 q) { slot := &p; *slot = q; "
                         "unsafe { **slot = 6; } }")
                     .diagnostics.HasErrors());
    EXPECT_NE(
        FindDiagnostic(Analyze("void f(*i32 p) { slot := &p; **slot = 6; }"),
                       DiagnosticCode::UnsafeOperation),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(
            Analyze("void f(constptr i32 p) { slot := &p; *slot = null; }"),
            DiagnosticCode::SymbolNotWritable),
        nullptr);
}

TEST(SemanticReferenceTests, TracksMovesAcrossBranchesLoopsAndJumps) {
    for (const std::string& body :
         {"if flag { moved := r; } return *r;",
          "while flag { moved := r; } return *r;",
          "while flag { #again moved := r; jumpto again; } return 0;",
          "while flag { if flag { moved := r; jumpto done; } #done return *r; "
          "} return 0;",
          "while flag { moved := r; proceed; } return 0;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(
            FindDiagnostic(Analyze("i32 f(bool flag) { i32 x = 1; r := &x; " +
                                   body + " }"),
                           DiagnosticCode::MovedReference),
            nullptr);
    }
    EXPECT_FALSE(Analyze("i32 f(bool flag) { i32 x = 1; r := &x; if flag { "
                         "moved := r; r = &x; } return *r; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("i32 f(bool flag) { i32 x = 1; while flag { r := &x; "
                         "moved := r; *moved = 2; } return x; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests, UnionsOriginsAtJoinsAndRetainsLoopBorrows) {
    EXPECT_NE(
        FindDiagnostic(Analyze("i32 f(bool flag) { i32 x = 1; i32 y = 2; r := "
                               "&x; if flag { r = &y; } x = 3; return *r; }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("i32 f(bool flag) { i32 x = 1; r := &x; while "
                               "flag { *r = 2; x = 3; } return 0; }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_FALSE(Analyze("i32 f(bool flag) { i32 x = 1; r := &x; while flag { "
                         "*r = 2; break; } x = 3; return x; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests,
     RejectsRebindingHandleDuringPendingIndirectAssignment) {
    EXPECT_NE(FindDiagnostic(Analyze("i32 read(&const i32 r) { return *r; }"
                                     "void f() { i32 x = 1; i32 y = 2; r := "
                                     "&x; *r = read(r = &y); }"),
                             DiagnosticCode::BorrowConflict),
              nullptr);
}

TEST(SemanticReferenceTests,
     ContextualIntegersCheckRangeWithoutConvertingTypedValues) {
    EXPECT_FALSE(
        Analyze("u8 twice(u8 x) { return x * 2; }"
                "i8 low() { return -((128)); }"
                "u64 high() { return 18446744073709551615; }"
                "i64 lowest() { return -9223372036854775808; }"
                "bool f() { u8 x = 255; x = 3; return twice(x) == 6; }")
            .diagnostics.HasErrors());
    for (const std::string& declaration :
         {"u8 x = 256;", "u8 x = -1;", "i8 x = 128;", "i8 x = -129;",
          "i64 x = 9223372036854775808;", "u64 x = 18446744073709551616;"}) {
        SCOPED_TRACE(declaration);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + declaration + " }"),
                                 DiagnosticCode::IntegerLiteralOutOfRange),
                  nullptr);
    }
    EXPECT_NE(
        FindDiagnostic(Analyze("void f() { i32 typed = 3; u8 x = typed; }"),
                       DiagnosticCode::IncompatibleType),
        nullptr);
}

TEST(SemanticReferenceTests, HandlesShortCircuitMovesAndUnreachableCode) {
    EXPECT_NE(FindDiagnostic(Analyze("bool take(&i32 a) { return true; }"
                                     "i32 f(bool flag) { i32 x = 1; i32 y = 2; "
                                     "r := &x; other := &y; "
                                     "flag and take(other = r); return *r; }"),
                             DiagnosticCode::MovedReference),
              nullptr);
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; r := &x; moved := r; return "
                         "*moved; *r = 2; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests, PreservesParentsAcrossMovedChildHandles) {
    EXPECT_NE(FindDiagnostic(Analyze("i32 f() { i32 x = 1; i32 y = 2; r := &x; "
                                     "child := &*r; moved := child; "
                                     "r = &y; return *moved; }"),
                             DiagnosticCode::BorrowConflict),
              nullptr);
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; i32 y = 2; r := &x; child := "
                         "&*r; moved := child; "
                         "*moved = 3; r = &y; *r = 4; return x + y; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests, CallLoansProtectAgainstMovesInLaterArguments) {
    EXPECT_NE(
        FindDiagnostic(Analyze("void take(&const i32 first, &i32 second) {}"
                               "void f() { i32 x = 1; i32 y = 2; r := &x; "
                               "other := &y; take(r, other = r); }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("i32 f(bool flag) { while flag { i32 seed "
                                     "= 0; r := &seed; #again "
                                     "i32 x = 1; if flag { r = &x; jumpto "
                                     "again; } return *r; } return 0; }"),
                             DiagnosticCode::BorrowConflict),
              nullptr);
}

TEST(SemanticReferenceTests, AnonymousReborrowsAndLexicalScopeEnds) {
    EXPECT_FALSE(Analyze("void take(&i32 r) { *r = 6; }"
                         "i32 f() { i32 x = 1; { r := &x; *r = 2; } "
                         "take(&*(&x)); return x; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("i32 f() { i32 x = 1; &const i32 r = &x; i32 y = 2; "
                         "copy := r; r = &y; return *copy + *r; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticReferenceTests,
     ReportsBorrowErrorsAfterIndependentFunctionErrors) {
    Analysis analysis = Analyze(
        "void bad() { &i32 missing; }"
        "i32 moved() { i32 x = 1; r := &x; next := r; return *r; }");
    EXPECT_NE(
        FindDiagnostic(analysis, DiagnosticCode::ReferenceRequiresInitializer),
        nullptr);
    EXPECT_NE(FindDiagnostic(analysis, DiagnosticCode::MovedReference),
              nullptr);
}

TEST(SemanticArrayTests, InfersElementsAndPreservesLengthInTypeIdentity) {
    Analysis analysis = Analyze(
        "void f() { inferred := [1, 2, 3]; [3]u8 bytes = [1, 2, 3]; "
        "repeated := [true; 4]; [0]u8 empty = []; [2][2]u8 grid = [[1, 2], [3, "
        "4]]; }");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const auto& function = FunctionAt(analysis, 0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 0))->Type().Name(),
        "[3]i32");
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type().Name(),
        "[3]u8");
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 2))->Type().Name(),
        "[4]bool");
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 3))->Type().Length(),
        0);
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(VariableAt(function, 4))->Type().Name(),
        "[2][2]u8");
    const SemanticType byte(SemanticTypeKind::U8);
    const SemanticType first(SemanticTypeKind::Array, byte, false, 3);
    const SemanticType second(SemanticTypeKind::Array, byte, false, 4);
    EXPECT_FALSE(IsAssignable(first, second));
    EXPECT_TRUE(IsAssignable(first, first.WithConst()));
    EXPECT_FALSE(CanExplicitlyCast(first, first));
}

TEST(SemanticArrayTests,
     SupportsWholeInitializationCopyReturnsAndElementWrites) {
    EXPECT_FALSE(
        Analyze("[2]u8 make() { return [1, 2]; }"
                "[2]u8 change([2]u8 copy) { copy[0] = 9; return copy; }"
                "usize f(bool flag) { [2]u8 a; if flag { a = make(); } else { "
                "a = none; } "
                "copy := change(a); a[0] = copy[1]; return a.len(); }")
            .diagnostics.HasErrors());
    EXPECT_FALSE(
        Analyze("void f() { [1]*u8 a = [null]; [1]*u8 b = [none; 1]; b = a; }")
            .diagnostics.HasErrors());
    for (const std::string& body :
         {"[2]u8 a; a[0] = 1;", "[2]u8 a; a.len();", "[2]u8 a; r := &a[0];",
          "[2]u8 a; copy := a;", "[2]u8 a; while true { a[0] = 1; break; }"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + body + " }"),
                                 DiagnosticCode::UninitializedVariable),
                  nullptr);
    }
}

TEST(SemanticArrayTests,
     RejectsMismatchedLengthsElementsAndUnsupportedStorage) {
    const std::vector<std::pair<std::string, DiagnosticCode>> cases{
        {"[2]u8 a = [1];", DiagnosticCode::ArrayInitializerCount},
        {"[2]u8 a = [1; 3];", DiagnosticCode::ArrayInitializerCount},
        {"[2]u8 a = [];", DiagnosticCode::ArrayInitializerCount},
        {"[1]u8 a = [256];", DiagnosticCode::IntegerLiteralOutOfRange},
        {"i32 x = 1; [1]u8 a = [x];", DiagnosticCode::IncompatibleType},
        {"a := [];", DiagnosticCode::CannotInferType},
        {"a := [none];", DiagnosticCode::InvalidArrayElement},
        {"a := [null];", DiagnosticCode::InvalidArrayElement},
        {"[1]string a = none;", DiagnosticCode::InvalidArrayElement},
        {"a := [\"view\"];", DiagnosticCode::InvalidArrayElement},
        {"[0]void a = none;", DiagnosticCode::InvalidArrayElement},
        {"i32 x = 1; a := [&x];", DiagnosticCode::InvalidArrayElement},
        {"[1]const u8 a = [1];", DiagnosticCode::InvalidArrayElement},
        {"[1]constptr u8 a = [null];", DiagnosticCode::InvalidArrayElement},
        {"[2][1]string a = none;", DiagnosticCode::InvalidArrayElement},
        {"[18446744073709551615]u8 a = none;",
         DiagnosticCode::InvalidArrayLength},
        {"a := [1; 18446744073709551615];", DiagnosticCode::InvalidArrayLength},
        {"[1000000000][1000000000]u64 a = none;",
         DiagnosticCode::InvalidArrayLength},
        {"[0][18446744073709551615]u8 a = none;",
         DiagnosticCode::InvalidArrayLength},
        {"[1]u8 a = [1]; a == a;", DiagnosticCode::InvalidOperand},
        {"[1]u8 a = [1]; a as [1]u8;", DiagnosticCode::InvalidCast},
        {"[1]u8 a = [1]; a.len(1);", DiagnosticCode::WrongArgumentCount},
    };
    for (const auto& [body, code] : cases) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + body + " }"), code),
                  nullptr);
    }
}

TEST(SemanticArrayTests, RequiresUsizeIndicesAndRejectsKnownOutOfBounds) {
    EXPECT_FALSE(Analyze("u8 f(usize i) { [2]u8 a = [1, 2]; return a[i]; }")
                     .diagnostics.HasErrors());
    EXPECT_NE(FindDiagnostic(
                  Analyze("u8 f(i32 i) { [2]u8 a = [1, 2]; return a[i]; }"),
                  DiagnosticCode::IncompatibleArgument),
              nullptr);
    for (const std::string& index :
         {"2", "(3)", "1 + 1", "2 * 3", "4 / 2", "5 - 3"}) {
        const std::string source =
            "u8 f() { [2]u8 a = [1, 2]; return a[" + index + "]; }";
        SCOPED_TRACE(source);
        Analysis analysis = Analyze(source);
        const Diagnostic* diagnostic =
            FindDiagnostic(analysis, DiagnosticCode::ArrayIndexOutOfBounds);
        ASSERT_NE(diagnostic, nullptr);
        EXPECT_EQ(source.substr(diagnostic->range.start.offset,
                                diagnostic->range.Length()),
                  index);
    }
    EXPECT_NE(
        FindDiagnostic(Analyze("u8 f(usize i) { [0]u8 a = []; return a[i]; }"),
                       DiagnosticCode::ArrayIndexOutOfBounds),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("u8 f() { [2]u8 a = none; return a[-1]; }"),
                       DiagnosticCode::IntegerLiteralOutOfRange),
        nullptr);
}

TEST(SemanticArrayTests,
     EnforcesConstThroughNestedElementsAndSharedReferences) {
    for (const std::string& body :
         {"const [2]u8 a = [1, 2]; a[0] = 3;",
          "const [2]u8 a = [1, 2]; a = [3, 4];",
          "const [2][1]u8 a = [[1], [2]]; a[0][0] = 3;",
          "const [2]u8 a = [1, 2]; r := &a[0]; *r = 3;",
          "[2]u8 a = [1, 2]; &const [2]u8 r = &a; (*r)[0] = 3;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + body + " }"),
                                 DiagnosticCode::SymbolNotWritable),
                  nullptr);
    }
    EXPECT_FALSE(
        Analyze("void f() { const [2]u8 a = [1, 2]; copy := a; copy[0] = 3; }")
            .diagnostics.HasErrors());
    EXPECT_FALSE(
        Analyze("unsafe void f(*u8 p) { const [1]*u8 a = [p]; *a[0] = 3; }")
            .diagnostics.HasErrors());
}

TEST(SemanticArrayTests,
     BorrowsWholeArraysAndElementsWithoutEscapingTemporaries) {
    EXPECT_FALSE(Analyze("void set(&u8 p) { *p = 6; } void clear(&[2]u8 a) { "
                         "(*a)[0] = 0; (*a)[1] = 0; }"
                         "u8 f() { [2]u8 a = [1, 2]; set(&a[0]); r := &a[1]; "
                         "*r = 4; clear(&a); return a[0]; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("u8 f() { [2]u8 a = [1, 2]; r := &a; child := "
                         "&(*r)[0]; *child = 4; (*r)[1] = 5; return a[0]; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("u8 f() { [2]u8 a = [1, 2]; &const u8 first = &a[0]; "
                         "&const u8 second = &a[1]; return *first + *second; }")
                     .diagnostics.HasErrors());
    for (const std::string& body :
         {"r := &a[0]; a[1] = 3; return *r;",
          "r := &a[0]; copy := a; return *r;",
          "r := &a[0]; other := &a[1]; return *r + *other;",
          "r := &a[0]; a = [3, 4]; return *r;",
          "r := &a; child := &(*r)[0]; (*r)[1] = 3; return *child;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(
            FindDiagnostic(Analyze("u8 f() { [2]u8 a = [1, 2]; " + body + " }"),
                           DiagnosticCode::BorrowConflict),
            nullptr);
    }
    EXPECT_NE(FindDiagnostic(Analyze("void f() { r := &[1, 2][0]; }"),
                             DiagnosticCode::InvalidReference),
              nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("void f() { [1, 2][0] = 3; }"),
                             DiagnosticCode::SemanticInvalidAssignmentTarget),
              nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("void f() { u8 x = 1; r := &x; { [1]u8 a "
                                     "= [2]; r = &a[0]; } }"),
                             DiagnosticCode::ReferenceEscape),
              nullptr);
}

TEST(SemanticArrayTests,
     TracksArrayBorrowsAcrossJoinsLoopsAndLiteralEvaluation) {
    EXPECT_NE(
        FindDiagnostic(Analyze("u8 f(bool flag) { [2]u8 a = [1, 2]; [2]u8 b = "
                               "[3, 4]; r := &a[0]; "
                               "if flag { r = &b[0]; } a[1] = 5; return *r; }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("void f(bool flag) { [2]u8 a = [1, 2]; r := "
                               "&a[0]; while flag { *r = 3; a[1] = 4; } }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("void f() { i32 x; a := [x; 0]; }"),
                             DiagnosticCode::UninitializedVariable),
              nullptr);
    EXPECT_NE(
        FindDiagnostic(
            Analyze("void f() { i32 x = 1; r := &x; moved := r; a := [*r]; }"),
            DiagnosticCode::MovedReference),
        nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("u8 read(&u8 p) { return *p; } u8 f() { "
                                     "[2]u8 a = [1, 2]; r := &a[0]; "
                                     "values := [read(&a[1])]; return *r; }"),
                             DiagnosticCode::BorrowConflict),
              nullptr);
}

TEST(SemanticArrayTests, PreservesRawPointerBoundaryForArrayElements) {
    EXPECT_FALSE(
        Analyze("unsafe void f() { [2]u8 a = [1, 2]; p := &raw a[0]; *p = 6; }")
            .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("unsafe void f(*[2]u8 p) { (*p)[0] = 6; }")
                     .diagnostics.HasErrors());
    EXPECT_NE(FindDiagnostic(Analyze("void f(*[2]u8 p) { (*p)[0] = 6; }"),
                             DiagnosticCode::UnsafeOperation),
              nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("unsafe void f(*[2]u8 p) { r := &(*p)[0]; }"),
                       DiagnosticCode::InvalidReference),
        nullptr);
    EXPECT_NE(FindDiagnostic(Analyze("unsafe void f() { [2]u8 a = none; r := "
                                     "&a; p := &raw (*r)[0]; }"),
                             DiagnosticCode::ReferenceEscape),
              nullptr);
}

TEST(SemanticArrayTests,
     FreezesAddressingReferenceWhileEvaluatingIndexAndStoreValue) {
    const std::string prefix =
        "usize index(&[2]u8 a) { return 0; } u8 read(&[2]u8 a) { return "
        "(*a)[0]; } "
        "void f() { [2]u8 a = [1, 2]; [2]u8 b = [3, 4]; ref := &a; ";
    for (const std::string& access :
         {"(*ref)[index(ref = &b)] = 7;", "(*ref)[0] = read(ref = &b);",
          "u8 value = (*ref)[index(ref = &b)];"}) {
        SCOPED_TRACE(access);
        EXPECT_NE(FindDiagnostic(Analyze(prefix + access + " }"),
                                 DiagnosticCode::BorrowConflict),
                  nullptr);
    }
}

TEST(SemanticArrayTests, PreservesDeepConstAndArrayShapeDuringReborrowing) {
    EXPECT_NE(FindDiagnostic(
                  Analyze("unsafe void f(*const [2]u8 p) { (*p)[0] = 3; }"),
                  DiagnosticCode::SymbolNotWritable),
              nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("void f(&[2]u8 p) { [3]u8 a = none; p = &a; }"),
                       DiagnosticCode::IncompatibleType),
        nullptr);
    EXPECT_FALSE(Analyze("u8 read(&const [2]u8 p) { return (*p)[0]; }"
                         "u8 f() { [2]u8 a = [1, 2]; constref [2]u8 r = &a; "
                         "(*r)[0] = 3; return read(r); }")
                     .diagnostics.HasErrors());
}

TEST(SemanticAnalyzerTests, AcceptsRangeBindingsEmptyArraysAndCountedLoops) {
    Analysis result = Analyze(R"(
        void f() {
            a := [1, 2];
            for i := range a { usize copy = i; }
            for (i, v) := range a { usize index = i; i32 value = v; }
            for (u32 i, i32 v) = range a { i = 4; v = 5; }
            for (_, v) := range a { v++; }
            for (_, _) := range a {}
            [0]u8 empty = [];
            for (u8 i, u8 v) = range empty { v++; }
            for (i, row) := range [[1, 2], [3, 4]] { row[0]++; }
            loop u32 x = 1; x <= 10; x++ { if x == 3 { proceed; } if x == 8 { break; } }
            loop i := 0; i < 2; i = i + 1 {}
            loop ;; { break; }
        }
    )");
    EXPECT_FALSE(result.diagnostics.HasErrors());
}

TEST(SemanticAnalyzerTests, ChecksRangeSourceBindingTypesAndIndexCapacity) {
    for (const std::string& source :
         {"void f() { for i := range 4 {} }",
          "void f() { for i := range \"abc\" {} }",
          "void f() { a := [1]; for (u32 i, u8 v) = range a {} }",
          "void f() { a := [1]; for (f64 i, i32 v) = range a {} }",
          "void f() { a := [1]; for (f64 _, i32 v) = range a {} }",
          "void f() { a := [1]; for (u32 i, u8 _) = range a {} }",
          "void f() { a := [0; 257]; for (u8 i, i32 v) = range a {} }",
          "void f() { a := [0; 129]; for (i8 i, i32 v) = range a {} }",
          "void f() { a := [1]; for (i, i) := range a {} }",
          "void f() { a := [1]; for (const u32 i, i32 v) = range a { i++; } }",
          "void f() { a := [1]; for (u32 i, const i32 v) = range a { v++; } "
          "}"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(
        Analyze("void f() { a := [0; 256]; for (u8 i, i32 v) = range a {} }")
            .diagnostics.HasErrors());
}

TEST(SemanticAnalyzerTests, ChecksLoopScopeInitializationAndUpdateEdges) {
    for (const std::string& source :
         {"void f() { loop i := 0; i < 2; i++ {} i++; }",
          "void f() { a := [1]; for (i, v) := range a {} v++; }",
          "void f() { i32 x; loop i := 0; i < 2; i++ { x = 2; } x++; }",
          "void f() { i32 x; loop i := 0; i < 2; x++ { proceed; } }",
          "void f() { [2]u8 a; for i := range a {} }",
          "void f() { loop i := 0; 1; i++ {} }",
          "void f() { loop i := 0; i < 2; local++ { local := 1; } }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(
        Analyze("void f() { i32 x; loop i := 0; i < 2; x++ { x = 1; i++; } }")
            .diagnostics.HasErrors());
}

TEST(SemanticAnalyzerTests, IncrementHonorsPlacesConstBorrowingAndUnsafe) {
    for (const std::string& source :
         {"void f() { i32 x; x++; }", "void f() { const i32 x = 1; x++; }",
          "void f() { (1 + 2)++; }", "void f() { [1, 2][0]++; }",
          "void f() { f64 x = 1.0; x++; }", "void f() { bool x = false; x++; }",
          "void f() { *u8 p = none; p++; }", "void f(*u8 p) { (*p)++; }",
          "void f() { x := 1; r := &x; x++; *r = 2; }",
          "void f() { x := 1; &const i32 r = &x; (*r)++; }",
          "void f() { const [2]i32 a = [1, 2]; a[0]++; }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(Analyze("void f() { x := 1; r := &x; (*r)++; unsafe { p := "
                         "&raw x; (*p)++; } }")
                     .diagnostics.HasErrors());
}

TEST(SemanticAnalyzerTests, RangeCopiesRespectLoansAndIterationLifetimes) {
    EXPECT_TRUE(
        Analyze(
            "void f() { a := [1, 2]; r := &a[0]; for i := range a {} *r = 3; }")
            .diagnostics.HasErrors());
    EXPECT_TRUE(Analyze("void f() { x := 1; r := &x; for (_, v) := range [1, "
                        "2] { r = &v; } *r = 3; }")
                    .diagnostics.HasErrors());
    EXPECT_FALSE(
        Analyze(
            "void f() { a := [1, 2]; for (i, v) := range a { a[i] = v + 1; } }")
            .diagnostics.HasErrors());
    EXPECT_TRUE(
        Analyze(
            "void f() { x := 1; r := &x; loop i := 0; i < 2; x++ { *r = i; } }")
            .diagnostics.HasErrors());
}

TEST(SemanticSliceTests, AcceptsCheckedSlicesCallsConstAndElementReferences) {
    const Analysis result = Analyze(R"(
        void bump([]i32 s) { s.front()++; s.back() = 4; }
        i32 read([]const i32 s) { return s.front(); }
        void f() {
            a := [1, 2, 3];
            s := a[:];
            bump(s); read(s);
            child := s[1:]; child[0] = 10;
            r := &s.back(); *r = 5;
            moved := s; moved[0] = 6;
            []const i32 shared = moved[:];
            copy := shared; read(copy); read(shared);
            const []i32 fixed = a[:]; fixed.front() = 1; bump(fixed);
            const [2]i32 frozen = [1, 2];
            inferred := frozen[:]; read(inferred);
            [0]i32 empty = []; empty_slice := empty[:]; empty_slice.len();
            matrix := [[1, 2], [3, 4]]; rows := matrix[:]; rows[0][1] = 7;
        }
    )");
    EXPECT_FALSE(result.diagnostics.HasErrors());
}

TEST(SemanticSliceTests, RejectsBadBoundsSourcesAndUnsupportedStorage) {
    for (const std::string& source :
         {"void f() { a := [1, 2]; s := a[2:1]; }",
          "void f() { a := [1, 2]; s := a[:3]; }",
          "void f() { a := [1, 2]; s := a[3:]; }",
          "void f() { a := [1, 2]; s := a[-1:]; }",
          "void f() { a := [1, 2]; u32 i = 0; s := a[i:]; }",
          "void f() { s := [1, 2][:]; }",
          "void f() { a := 2; s := a[:]; }",
          "void f() { s := \"abc\"[:]; }",
          "void f() { []u8 s; }",
          "void f() { []u8 s = none; }",
          "void f() { []u8 s = null; }",
          "void f([]string s) {}",
          "void f([]&u8 s) {}",
          "void f([][]u8 s) {}",
          "void f([2][]u8 a) {}",
          "void f(&[]u8 s) {}",
          "void f(*[]u8 s) {}",
          "[]u8 f() { [1]u8 a = [1]; return a[:]; }",
          "unsafe void f(*[2]u8 p) { s := (*p)[:]; }",
          "void f() { a := [1]; s := a[:]; unsafe { p := &raw s; } }",
          "void f() { a := [1]; s := a[:]; unsafe { p := &raw s.front(); } }",
          "void f() { [2]u8 a; s := a[:]; }",
          "void f() { a := [1]; s := a[:]; s.len() = 1; }",
          "void f() { a := [1]; s := a[:]; s.front(0); }",
          "void f() { a := [1]; s := a[:]; s == s; }",
          "void f() { a := [1]; s := a[:] as []i32; }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
}

TEST(SemanticSliceTests, EnforcesReadOnlyElementsAndFixedHandles) {
    for (const std::string& source :
         {"void f() { const [1]i32 a = [1]; []i32 s = a[:]; }",
          "void f() { a := [1]; []const i32 s = a[:]; s[0] = 2; }",
          "void f() { a := [1]; []const i32 s = a[:]; s.front()++; }",
          "void f() { a := [1]; []const i32 s = a[:]; []i32 mutable = s[:]; }",
          "void f() { a := [1]; const []i32 s = a[:]; s = a[:]; }",
          "void f() { a := [1]; const []i32 s = a[:]; moved := s; }",
          "void f() { a := [[1]]; []const [1]i32 s = a[:]; s[0][0]++; }",
          "void bump([]i32 s) {} void f() { a := [1]; []const i32 s = a[:]; "
          "bump(s); }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
}

TEST(SemanticSliceTests, TracksMovesLoansAndEscapesAcrossControlFlow) {
    for (const std::string& source :
         {"void f() { a := [1, 2]; s := a[:]; a[0] = 3; s[0] = 2; }",
          "void f() { a := [1, 2]; s := a[:]; a.len(); s.len(); }",
          "void f() { a := [1, 2]; s := a[:]; moved := s; s.len(); }",
          "void f(bool b) { a := [1, 2]; s := a[:]; if b { moved := s; } "
          "s.len(); }",
          "void f() { a := [1, 2]; s := a[:]; child := s[:1]; s[1] = 3; "
          "child[0] = 2; }",
          "void f() { a := [1, 2]; s := a[:]; child := s[:1]; moved := s; "
          "child.len(); }",
          "void f() { a := [1, 2]; s := a[:]; { inner := [3, 4]; s = inner[:]; "
          "} s.len(); }",
          "void f() { a := [1, 2]; s := a[:]; { parent := s[:]; s = parent[:]; "
          "} }",
          "void f() { a := [1, 2]; s := a[:]; r := &s[0]; s[1] = 2; *r = 3; }",
          "void f() { a := [1, 2]; s := a[:]; loop i := 0; i < 2; i++ { moved "
          ":= s; moved.len(); } }",
          "void use([]i32 x, []i32 y) {} void f() { a := [1, 2]; s := a[:]; "
          "use(s, s); }",
          "void use([]const i32 x, []i32 y) {} void f() { a := [1, 2]; s := "
          "a[:]; use(s, s); }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(Analyze("void use([]const i32 x, []const i32 y) {} void f() { "
                         "a := [1, 2]; s := a[:]; use(s, s); s[0]++; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("void f() { a := [1, 2]; s := a[:]; child := s[:1]; "
                         "child[0]++; s[1]++; a[0]++; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticSliceTests, PinsParentsDuringBoundsAndPendingStores) {
    for (const std::string& source :
         {"void f() { a := [1, 2]; b := [3, 4]; s := a[:]; t := b[:]; child := "
          "s[(s = t).len():]; }",
          "void f() { a := [1, 2]; b := [3, 4]; s := a[:]; t := b[:]; s[(s = "
          "t).len()] = 1; }",
          "void f() { a := [1, 2]; b := [3, 4]; s := a[:]; t := b[:]; s[0] = "
          "(s = t).front(); }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
}

TEST(SemanticSliceTests,
     RangesReborrowReadOnlyAndPreserveHandleAfterIteration) {
    EXPECT_FALSE(Analyze("void f() { a := [1, 2]; s := a[:]; for (i, v) := "
                         "range s { usize n = i; i32 x = v; } s[0]++; }")
                     .diagnostics.HasErrors());
    EXPECT_TRUE(Analyze("void f() { a := [1, 2]; s := a[:]; for (i, v) := "
                        "range s { s[i]++; } }")
                    .diagnostics.HasErrors());
    EXPECT_TRUE(Analyze("void f() { a := [1, 2]; s := a[:]; for (u8 i, i32 v) "
                        "= range s {} }")
                    .diagnostics.HasErrors());
}

TEST(SemanticDeferTests, CapturesAtRegistrationAndChecksCallsNormally) {
    Analysis result = Analyze(R"(
        void use(i32 x) {}
        i32 result() { return 1; }
        unsafe void raw(*i32 p) {}
        void f() {
            i32 x;
            defer use(x = 1);
            x++;
            defer result();
            defer (use(x));
            unsafe { defer raw(&raw x); }
            string word = "first";
            defer stdoutln(word.view());
            word = "second";
        }
    )");
    EXPECT_FALSE(result.diagnostics.HasErrors());
    for (const std::string& source :
         {"void f() { defer 42; }", "void f() { defer f; }",
          "void f() { a := [1]; defer a.len(); }",
          "void f() { defer missing(); }",
          "void g(i32 x) {} void f() { defer g(); }",
          "void g(i32 x) {} void f() { defer g(true); }",
          "void g(i32 x) {} void f() { i32 x; defer g(x); x = 1; }",
          "unsafe void g() {} void f() { defer g(); }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    const Analysis invalid = Analyze("void f() { defer 42; }");
    const auto* diagnostic =
        FindDiagnostic(invalid, DiagnosticCode::InvalidDefer);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->range.start.offset, 17);
    EXPECT_EQ(diagnostic->range.end.offset, 19);
}

TEST(SemanticDeferTests, HoldsBorrowedArgumentsUntilTheirScopeEnds) {
    for (const std::string& source :
         {"void g(&i32 r) {} void f() { x := 1; defer g(&x); x++; }",
          "void g(&i32 r) {} i32 f() { x := 1; defer g(&x); return x; }",
          "void g(&const i32 r) {} void f() { x := 1; defer g(&x); x++; }",
          "void g(&i32 r) {} void f() { x := 1; r := &x; defer g(r); moved := "
          "r; }",
          "void g(&i32 r) {} void f() { x := 1; y := 2; r := &x; defer g(r); r "
          "= &y; }",
          "void g([]i32 s) {} void f() { a := [1, 2]; defer g(a[:]); a[0]++; }",
          "void g([]const i32 s) {} void f() { a := [1]; s := a[:]; defer "
          "g(s); s[0]++; }",
          "void g(&i32 r) {} void f() { x := 1; defer g(&x); defer g(&x); }",
          "void g(&i32 r, i32 x) {} void f() { x := 1; defer g(&x, x++); }",
          "void g(&i32 r) {} void f(bool skip) { x := 1; while true { #again "
          "defer g(&x); if skip { jumpto again; } break; } }",
          "void g(&const i32 r) {} void f() { while true { #again x := 1; "
          "defer g(&x); jumpto again; } }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(Analyze("void g(&i32 r) {} void f() { x := 1; r := &x; { "
                         "defer g(r); } *r = 2; x++; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("void g(&const i32 r) {} i32 f() { x := 1; defer "
                         "g(&x); defer g(&x); return x; }")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("void g([]i32 s) {} void f() { a := [1]; s := a[:]; { "
                         "defer g(s); } s[0]++; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticDeferTests, TracksRegistrationAndLoanReleaseThroughJumps) {
    EXPECT_FALSE(Analyze(R"(
        void g(&i32 r) {}
        void f() {
            x := 1;
            while true {
                { defer g(&x); jumpto done; }
                #done
                x++;
                break;
            }
        }
    )")
                     .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("void g(&i32 r) {} void f() { x := 1; loop i := 0; i "
                         "< 2; i++ { defer g(&x); proceed; } x++; }")
                     .diagnostics.HasErrors());
    EXPECT_TRUE(
        Analyze("void g(&i32 r) {} void f(bool b) { x := 1; while true { if b "
                "{ jumpto done; } defer g(&x); #done x++; break; } }")
            .diagnostics.HasErrors());
    EXPECT_FALSE(Analyze("void g(&i32 r) {} void f() { x := 1; while true { "
                         "jumpto done; defer g(&x); #done x++; break; } }")
                     .diagnostics.HasErrors());
}

TEST(SemanticTupleTests, SupportsValuesReturnsParametersAndContextualElements) {
    const auto result = Analyze(R"(
        (u8, (bool, string)) f(u8 n) { return (n, (true, "text")); }
        (u8, (bool, string)) pass((u8, (bool, string)) p) { return p; }
        i32 main() {
            (u8, (bool, string)) t;
            t = pass(f(42));
            (n, nested) := t;
            (bool ok, string text) = nested;
            (n, (bool, string) copy) = (3, nested);
            (const u8 fixed, _) = t;
            const (u8, bool) frozen = (1, true);
            (u8, bool) zero = none;
            (u8 small, *i32 pointer) = (none, null);
            return n as i32;
        }
    )");
    EXPECT_FALSE(result.diagnostics.HasErrors());
    const SemanticType type =
        result.model.TypeOf(*static_cast<const ReturnStatement&>(
                                 *FunctionAt(result, 0).Body().Statements()[0])
                                 .Value());
    EXPECT_EQ(type.Name(), "(u8, (bool, string))");
    EXPECT_TRUE(type.OwnsStorage());
    EXPECT_TRUE(type.IsCompound());
}

TEST(SemanticTupleTests, ChecksArityTypesMutabilityAndUnsupportedElements) {
    for (const std::string& source :
         {"void f() { (a, b) := 1; }",
          "void f() { (a, b) := (1, 2, 3); }",
          "void f() { (a, a) := (1, 2); }",
          "void f() { a := 0; (a, a) = (1, 2); }",
          "void f() { (a, b) = (1, 2); }",
          "void f() { (i32 a, b) := (1, 2); }",
          "void f() { (bool a, i32 b) = (1, 2); }",
          "void f() { (u8 a, i32 b) = (256, 2); }",
          "void f() { const i32 a = 1; (a, _) = (2, 3); }",
          "void f() { const (i32, bool) a = (1, true); a = (2, false); }",
          "(i32, bool) f() { return (1, 2); }",
          "(i32, bool) f() { return 1; }",
          "void f() { t := (1, 2); t == t; }",
          "void f() { t := (1, 2); t as (i32, i32); }",
          "void f() { (void, i32) t; }",
          "void f() { x := 1; t := (&x, 2); }",
          "void f() { a := [1]; t := (a[:], 2); }",
          "void f() { t := (\"view\", 2); }",
          "void f() { (*&i32, i32) t; }",
          "void f() { [2](i32, bool) t; }",
          "void f() { (string, i32) t = (\"s\", 1); r := &t; }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    const std::string source = "void f() { (a, a) := (1, 2); }";
    const auto invalid = Analyze(source);
    const auto* diagnostic =
        FindDiagnostic(invalid, DiagnosticCode::InvalidDestructure);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->range.start.offset, source.find("a)"));
}

TEST(SemanticTupleTests, PreservesInitializationAndSimultaneousBindingRules) {
    for (const std::string& source :
         {"void f() { i32 a; i32 b; (a, b) = (1, 2); stdoutln(a + b); }",
          "void f() { i32 a; (i32 b, i32 c) = (a = 1, a); stdoutln(a); }",
          "void f() { a := 1; b := 2; (a, b) = (b, a); }",
          "void f() { a := 1; { (a, b) := (a, 2); stdoutln(a); } }",
          "void f() { (_, _) := (1, 2); }"}) {
        SCOPED_TRACE(source);
        EXPECT_FALSE(Analyze(source).diagnostics.HasErrors());
    }
    for (const std::string& source :
         {"void f() { i32 a; i32 b; (a, b) = (1, a); }",
          "void f() { (a, b) := (1, a); }",
          "void f(bool c) { i32 a; i32 b; if c { (a, b) = (1, 2); } "
          "stdoutln(a); }",
          "void f() { i32 a; while true { jumpto use; (a, i32 b) = (1, 2); "
          "#use stdoutln(a); break; } }",
          "void f() { (i32, bool) t; (a, b) := t; }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
}

TEST(SemanticTupleTests, DestructuringCannotWriteThroughActiveLoans) {
    for (const std::string& source :
         {"void use(&i32 r) {} void f() { x := 1; defer use(&x); (x, _) = (2, "
          "3); }",
          "void f() { x := 1; r := &x; (x, _) = (2, 3); stdoutln(*r); }",
          "void f() { a := [1]; r := &a[0]; (a, _) = ([2], 3); stdoutln(*r); }",
          "void f() { t := (1, 2); r := &t; (t, _) = ((2, 3), 4); p := *r; "
          "}"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
    EXPECT_FALSE(
        Analyze("void f() { t := (1, 2); r := &t; (a, b) := *r; t = (b, a); }")
            .diagnostics.HasErrors());
}

TEST(SemanticTupleTests, BareBindingGroupsInheritTypesAndQualifiers) {
    EXPECT_FALSE(Analyze(R"(
        (i32, i32) divide() { return (3, 2); }
        void f() {
            i32 q, r = divide();
            i32 first, i32 second = divide();
            a, b := divide();
            a, b = (b, a);
            bool ok, changed, i32 code = (true, false, 42);
            const u8 low, high = (0, 255);
            (i32, bool) left, right = ((42, true), (7, false));
            q, i32 fresh, another = (1, 2, 3);
            _, b = divide();
            (i32 explicit_new, q) = divide();
        }
    )")
                     .diagnostics.HasErrors());
    for (const std::string& source :
         {"void f() { i32 q, r = (1, true); }",
          "void f() { u8 q, r = (1, 256); }",
          "void f() { const i32 q, r = (1, 2); r = 3; }",
          "void f() { i32 q, r := (1, 2); }",
          "void f() { i32 q, r = (1, 2, 3); }", "void f() { q, r = (1, 2); }",
          "void f() { i32 q, q = (1, 2); }",
          "void f() { r := 0; i32 q, r = (1, 2); }",
          "void f() { i32 q, r = (1, q); }",
          "void f() { string q, r = (\"one\", 2); }"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(Analyze(source).diagnostics.HasErrors());
    }
}

TEST(SemanticErrorTests, ResolvesForwardFamiliesAndPreservesNominalIdentity) {
    Analysis analysis = Analyze(
        "Fault f(Fault x) { return x; } "
        "error Fault { Bad, Worse } error Other { Bad } "
        "void use() { first := Fault.Bad; second := Other.Bad; "
        "error any = first; error success = none; Fault ok = none; "
        "Fault a, b = (first, ok); }");
    ASSERT_FALSE(analysis.diagnostics.HasErrors());
    const auto& function = FunctionAt(analysis, 3);
    const auto first =
        analysis.model.DeclaredSymbol(VariableAt(function, 0))->Type();
    const auto second =
        analysis.model.DeclaredSymbol(VariableAt(function, 1))->Type();
    const auto generic =
        analysis.model.DeclaredSymbol(VariableAt(function, 2))->Type();
    EXPECT_EQ(first.Name(), "Fault");
    EXPECT_EQ(generic.Name(), "error");
    EXPECT_TRUE(first.IsErrorValue());
    EXPECT_FALSE(first.IsError());
    EXPECT_NE(first, second);
    EXPECT_TRUE(IsAssignable(generic, first));
    EXPECT_FALSE(IsAssignable(first, generic));
    EXPECT_FALSE(IsAssignable(first, second));
    EXPECT_EQ(
        analysis.model.DeclaredSymbol(*FunctionAt(analysis, 0).Parameters()[0])
            ->Type(),
        first);
    EXPECT_FALSE(first.OwnsStorage());
    EXPECT_FALSE(generic.OwnsStorage());
}

TEST(SemanticErrorTests, ChecksErrorOperatorsAndConversions) {
    const std::string prefix = "error Fault { Bad } error Other { Bad } ";
    for (const std::string& expression :
         {"Fault.Bad == none", "none != Fault.Bad", "Fault.Bad == Fault.Bad",
          "a == Fault.Bad", "Fault.Bad != a", "a == b", "a === b",
          "Fault.Bad === Other.Bad", "Fault.Bad !== none", "a === Fault.Bad"}) {
        SCOPED_TRACE(expression);
        EXPECT_FALSE(Analyze(prefix + "bool f(error a, error b) { return " +
                             expression + "; }")
                         .diagnostics.HasErrors());
    }
    for (const std::string& expression :
         {"Fault.Bad == Other.Bad", "Fault.Bad != Other.Bad", "a + b", "a < b",
          "a == 0", "a == false", "a == null", "!a", "a and b"}) {
        SCOPED_TRACE(expression);
        EXPECT_NE(
            FindDiagnostic(Analyze(prefix + "void f(error a, error b) { " +
                                   expression + "; }"),
                           DiagnosticCode::InvalidOperand),
            nullptr);
    }
    for (const std::string& body :
         {"Fault value = a;", "Fault value = Other.Bad;", "i32 value = a;",
          "error value = 1;", "error value = null;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(
            FindDiagnostic(Analyze(prefix + "void f(error a) { " + body + " }"),
                           DiagnosticCode::IncompatibleType),
            nullptr);
    }
    for (const std::string& expression :
         {"a as Fault", "Fault.Bad as i32", "1 as error", "a as string"}) {
        SCOPED_TRACE(expression);
        EXPECT_NE(FindDiagnostic(Analyze(prefix + "void f(error a) { " +
                                         expression + "; }"),
                                 DiagnosticCode::InvalidCast),
                  nullptr);
    }
    EXPECT_NE(FindDiagnostic(Analyze(prefix + "void f(error a) { if a {} }"),
                             DiagnosticCode::InvalidCondition),
              nullptr);
}

TEST(SemanticErrorTests, ReportsInvalidFamiliesNamesAndVariantsAtTheirRanges) {
    for (const std::string& source :
         {"error Fault { Bad, Bad }",
          "error Fault { Bad } error Fault { Worse }",
          "error Fault { Bad } void Fault() {}", "error stdout { Bad }"}) {
        SCOPED_TRACE(source);
        EXPECT_NE(FindDiagnostic(Analyze(source),
                                 DiagnosticCode::DuplicateDeclaration),
                  nullptr);
    }
    EXPECT_NE(FindDiagnostic(Analyze("error Empty {}"),
                             DiagnosticCode::InvalidErrorDeclaration),
              nullptr);
    const std::string source =
        "error Fault { Bad } void f() { Fault.Missing; }";
    Analysis missing = Analyze(source);
    const Diagnostic* diagnostic =
        FindDiagnostic(missing, DiagnosticCode::InvalidMember);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->range.start.offset, source.find("Missing"));
    EXPECT_EQ(diagnostic->range.Length(), 7);
    for (const std::string& body :
         {"Unknown value = none;", "*Unknown value = none;",
          "(error, Unknown) value = none;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(Analyze("void f() { " + body + " }"),
                                 DiagnosticCode::UnknownType),
                  nullptr);
    }
    for (const std::string& body : {"Unknown.Bad;", "i32 Fault = 1; Fault.Bad;",
                                    "Fault value = none; value.Bad;"}) {
        SCOPED_TRACE(body);
        EXPECT_NE(FindDiagnostic(
                      Analyze("error Fault { Bad } void f() { " + body + " }"),
                      DiagnosticCode::InvalidMember),
                  nullptr);
    }
    EXPECT_NE(FindDiagnostic(
                  Analyze("error Fault { Bad } void f() { value := Fault; }"),
                  DiagnosticCode::InvalidOperand),
              nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze("error Fault { Bad } void f() { Fault.Bad(); }"),
                       DiagnosticCode::InvalidOperand),
        nullptr);
    EXPECT_FALSE(Analyze("error Fault { Bad } Fault f(Fault Fault, Fault next) "
                         "{ return next; }")
                     .diagnostics.HasErrors());
}

TEST(SemanticErrorTests, IntegratesDefiniteAssignmentQualifiersAndBorrowing) {
    const std::string prefix = "error Fault { Bad } ";
    EXPECT_NE(
        FindDiagnostic(Analyze(prefix + "error f() { error e; return e; }"),
                       DiagnosticCode::UninitializedVariable),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze(prefix + "Fault f(bool flag) { Fault e; if flag "
                                        "{ e = Fault.Bad; } return e; }"),
                       DiagnosticCode::UninitializedVariable),
        nullptr);
    EXPECT_FALSE(Analyze(prefix + "error f(bool flag) { error e; if flag { e = "
                                  "Fault.Bad; } else { e = none; } return e; }")
                     .diagnostics.HasErrors());
    EXPECT_NE(FindDiagnostic(
                  Analyze(prefix +
                          "void f() { const error e = none; e = Fault.Bad; }"),
                  DiagnosticCode::SymbolNotWritable),
              nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze(prefix + "void f() { error e = none; &const "
                                        "error r = &e; *r = Fault.Bad; }"),
                       DiagnosticCode::SymbolNotWritable),
        nullptr);
    EXPECT_NE(
        FindDiagnostic(Analyze(prefix + "void f() { error e = none; r := &e; e "
                                        "= Fault.Bad; copy := *r; }"),
                       DiagnosticCode::BorrowConflict),
        nullptr);
    EXPECT_FALSE(Analyze(prefix + "void f() { [2]error a = [Fault.Bad, none]; "
                                  "{ []error s = a[:]; s[0] = none; } "
                                  "(Fault, error) pair = (Fault.Bad, a[0]); "
                                  "Fault x, error y = pair; }")
                     .diagnostics.HasErrors());
}
