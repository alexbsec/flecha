#include "semantic/semantic_analyzer.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/token_type.hpp"
#include "semantic/borrow_checker.hpp"
#include "semantic/definite_assignment.hpp"

namespace flecha::semantic {
namespace {

std::string TypeMismatch(SemanticType expected, SemanticType actual) {
    return "expected '" + std::string(expected.Name()) + "', found '" +
           std::string(actual.Name()) + "'";
}

bool IsArrayElement(SemanticType type) {
    if (type.IsConst()) return false;
    if (type.Kind() == SemanticTypeKind::Array)
        return IsArrayElement(*type.Target());
    return type.IsNumeric() || type.Kind() == SemanticTypeKind::Bool ||
           type.Kind() == SemanticTypeKind::Char ||
           type.Kind() == SemanticTypeKind::Pointer;
}

// Keep byte and bit layout arithmetic representable by the native target and
// LLVM.
std::optional<std::uint64_t> StorageSize(SemanticType type) {
    constexpr std::uint64_t MAX_BYTES =
        std::numeric_limits<std::ptrdiff_t>::max() / 8;
    if (type.Kind() == SemanticTypeKind::Array) {
        const std::optional<std::uint64_t> element =
            StorageSize(*type.Target());
        if (!element || type.Length() > MAX_BYTES ||
            (*element != 0 && type.Length() > MAX_BYTES / *element))
            return std::nullopt;
        return type.Length() * *element;
    }
    switch (type.Kind()) {
        case SemanticTypeKind::I8:
        case SemanticTypeKind::U8:
        case SemanticTypeKind::Bool:
        case SemanticTypeKind::Char:
            return 1;
        case SemanticTypeKind::I16:
        case SemanticTypeKind::U16:
            return 2;
        case SemanticTypeKind::I32:
        case SemanticTypeKind::U32:
        case SemanticTypeKind::F32:
            return 4;
        case SemanticTypeKind::I64:
        case SemanticTypeKind::U64:
        case SemanticTypeKind::F64:
            return 8;
        case SemanticTypeKind::ISize:
        case SemanticTypeKind::USize:
        case SemanticTypeKind::Pointer:
            return sizeof(void*);
        default:
            return std::nullopt;
    }
}

std::optional<std::uint64_t> ConstantIndex(const core::Expression& expression) {
    if (expression.Kind() == core::AstNodeKind::ParenthesizedExpression)
        return ConstantIndex(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    if (expression.Kind() == core::AstNodeKind::LiteralExpression) {
        const auto& literal =
            static_cast<const core::LiteralExpression&>(expression);
        if (literal.LiteralType() != TokenType::NumberLiteral)
            return std::nullopt;
        std::uint64_t value = 0;
        const std::string& text = literal.Value();
        const auto result =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec == std::errc{} && result.ptr == text.data() + text.size())
            return value;
    }
    if (expression.Kind() == core::AstNodeKind::UnaryExpression) {
        const auto& unary =
            static_cast<const core::UnaryExpression&>(expression);
        if (unary.OperatorType() == TokenType::Add)
            return ConstantIndex(unary.Operand());
    }
    if (expression.Kind() == core::AstNodeKind::BinaryExpression) {
        const auto& binary =
            static_cast<const core::BinaryExpression&>(expression);
        const std::optional<std::uint64_t> left = ConstantIndex(binary.Left());
        const std::optional<std::uint64_t> right =
            ConstantIndex(binary.Right());
        if (!left || !right) return std::nullopt;
        constexpr std::uint64_t MAX =
            std::numeric_limits<std::uintptr_t>::max();
        switch (binary.OperatorType()) {
            case TokenType::Add:
                if (*left <= MAX - *right) return *left + *right;
                break;
            case TokenType::Sub:
                if (*left >= *right) return *left - *right;
                break;
            case TokenType::Mul:
                if (*right == 0 || *left <= MAX / *right) return *left * *right;
                break;
            case TokenType::Div:
                if (*right != 0) return *left / *right;
                break;
            case TokenType::Mod:
                if (*right != 0) return *left % *right;
                break;
            default:
                break;
        }
    }
    return std::nullopt;
}

}  // namespace

void SemanticAnalyzer::Report(diagnostics::DiagnosticCode code,
                              source::SourceRange range, std::string message) {
    _diagnostics.Report(code, range, std::move(message));
}

Scope& SemanticAnalyzer::AddScope(const Scope* parent) {
    _model._scopes.push_back(std::make_unique<Scope>(parent));
    return *_model._scopes.back();
}

const Symbol* SemanticAnalyzer::AddSymbol(std::unique_ptr<Symbol> symbol,
                                          const core::AstNode& declaration) {
    const Symbol* result = symbol.get();
    _model._symbols.push_back(std::move(symbol));
    _model._declarations.emplace(&declaration, result);
    if (!_currentScope->Declare(*result)) {
        Report(diagnostics::DiagnosticCode::DuplicateDeclaration,
               result->DeclarationRange(),
               "duplicate declaration of '" + result->Name() + "'");
        return nullptr;
    }
    return result;
}

SemanticType SemanticAnalyzer::ResolveType(const core::TypeNode& type) {
    const SemanticType result = SemanticType::FromSyntax(type);
    const SemanticType* layer = &result;
    if (layer->Kind() == SemanticTypeKind::Reference) {
        layer = layer->Target();
        if (layer->Kind() == SemanticTypeKind::Void ||
            layer->Kind() == SemanticTypeKind::String ||
            layer->Kind() == SemanticTypeKind::StringView) {
            Report(
                diagnostics::DiagnosticCode::UnsupportedCompoundType,
                type.Range(),
                "references currently require scalar or raw-pointer storage");
        }
    }
    while (layer->IsCompound()) {
        if (layer->Kind() == SemanticTypeKind::Reference) {
            Report(diagnostics::DiagnosticCode::UnsupportedCompoundType,
                   type.Range(),
                   "nested reference storage is not yet supported: '" +
                       result.Name() + "'");
            break;
        }
        if (layer->Kind() == SemanticTypeKind::Array)
            ValidateArrayType(*layer, type.Range());
        layer = layer->Target();
    }
    return result;
}

void SemanticAnalyzer::RegisterBuiltIns() {
    const auto registerOutput = [this](std::string name,
                                       BuiltInFunction builtIn) {
        std::unique_ptr<FunctionSymbol> function =
            std::make_unique<FunctionSymbol>(
                std::move(name), SemanticType(SemanticTypeKind::Void),
                std::vector<SemanticType>{
                    SemanticType(SemanticTypeKind::StringView)},
                source::SourceRange{}, builtIn);
        const FunctionSymbol* symbol = function.get();
        _model._symbols.push_back(std::move(function));
        (void)_currentScope->Declare(*symbol);
    };
    registerOutput("stdout", BuiltInFunction::Stdout);
    registerOutput("stdoutln", BuiltInFunction::StdoutLine);
}

void SemanticAnalyzer::RegisterFunctions(const core::CompilationUnit& unit) {
    for (const std::unique_ptr<core::Declaration>& declaration :
         unit.Declarations()) {
        if (declaration->Kind() != core::AstNodeKind::FunctionDeclaration) {
            continue;
        }
        const core::FunctionDeclaration& function =
            static_cast<const core::FunctionDeclaration&>(*declaration);
        std::vector<SemanticType> parameterTypes;
        parameterTypes.reserve(function.Parameters().size());
        for (const std::unique_ptr<core::Parameter>& parameter :
             function.Parameters()) {
            parameterTypes.push_back(ResolveType(parameter->Type()));
        }
        const SemanticType returnType = ResolveType(function.ReturnType());
        if (returnType.Kind() == SemanticTypeKind::Reference) {
            Report(diagnostics::DiagnosticCode::ReferenceEscape,
                   function.ReturnType().Range(),
                   "reference returns require a lifetime contract and are not "
                   "yet supported");
        }
        AddSymbol(std::make_unique<FunctionSymbol>(
                      function.Name(), returnType, std::move(parameterTypes),
                      function.NameRange(), BuiltInFunction::None,
                      function.IsUnsafe()),
                  function);
    }
}

SemanticModel SemanticAnalyzer::Analyze(const core::CompilationUnit& unit) {
    _model = SemanticModel{};
    _currentScope = &AddScope(nullptr);
    RegisterBuiltIns();
    RegisterFunctions(unit);
    for (const std::unique_ptr<core::Declaration>& declaration :
         unit.Declarations()) {
        if (declaration->Kind() == core::AstNodeKind::FunctionDeclaration) {
            AnalyzeFunction(
                static_cast<const core::FunctionDeclaration&>(*declaration));
        }
    }
    _currentScope = nullptr;
    _currentFunction = nullptr;
    return std::move(_model);
}

void SemanticAnalyzer::AnalyzeFunction(
    const core::FunctionDeclaration& function) {
    const std::size_t diagnosticsBefore = _diagnostics.Diagnostics().size();
    const Symbol* functionBase = _model.DeclaredSymbol(function);
    if (functionBase != nullptr &&
        functionBase->Kind() == SymbolKind::Function) {
        _currentFunction = static_cast<const FunctionSymbol*>(functionBase);
    } else {
        _currentFunction = nullptr;
    }

    _positions.clear();
    _localStringBorrows.clear();
    _loopDepth = 0;
    _inUnsafe = function.IsUnsafe();
    CollectPositions(function.Body(), 0);

    Scope* previousScope = _currentScope;
    _currentScope = &AddScope(previousScope);
    for (const std::unique_ptr<core::Parameter>& parameter :
         function.Parameters()) {
        AddSymbol(std::make_unique<VariableSymbol>(
                      SymbolKind::Parameter, parameter->Name(),
                      SemanticType::FromSyntax(parameter->Type()),
                      parameter->NameRange(), !parameter->Type().IsConst()),
                  *parameter);
    }
    AnalyzeBlock(function.Body(), false);
    const ControlFlowGraph graph = BuildControlFlowGraph(function, _model);
    AnalyzeDefiniteAssignment(function, _model, _diagnostics, graph);
    if (_diagnostics.Diagnostics().size() == diagnosticsBefore)
        AnalyzeBorrows(function, _model, _diagnostics, graph);
    _currentScope = previousScope;
    _currentFunction = nullptr;
    _inUnsafe = false;
}

void SemanticAnalyzer::CollectPositions(const core::BlockStatement& block,
                                        std::size_t loopDepth) {
    for (const std::unique_ptr<core::Statement>& statement :
         block.Statements()) {
        if (statement->Kind() == core::AstNodeKind::BlockStatement) {
            CollectPositions(
                static_cast<const core::BlockStatement&>(*statement),
                loopDepth);
            continue;
        }
        if (statement->Kind() == core::AstNodeKind::RangeStatement) {
            CollectPositions(
                static_cast<const core::RangeStatement&>(*statement)
                    .Expansion(),
                loopDepth);
            continue;
        }
        if (statement->Kind() == core::AstNodeKind::PositionStatement) {
            const core::PositionStatement& position =
                static_cast<const core::PositionStatement&>(*statement);
            if (loopDepth == 0) {
                Report(diagnostics::DiagnosticCode::LoopControlOutsideLoop,
                       position.Range(),
                       "positions are only valid inside a loop");
            }
            if (!_positions.emplace(position.Name(), position.NameRange())
                     .second) {
                Report(diagnostics::DiagnosticCode::DuplicatePosition,
                       position.NameRange(),
                       "duplicate position '" + position.Name() + "'");
            }
            continue;
        }
        if (statement->Kind() == core::AstNodeKind::IfStatement) {
            const core::IfStatement& ifStatement =
                static_cast<const core::IfStatement&>(*statement);
            CollectPositions(ifStatement.PrimaryBranch().Body(), loopDepth);
            if (ifStatement.PrimaryBranch().Also() != nullptr) {
                CollectPositions(ifStatement.PrimaryBranch().Also()->Body(),
                                 loopDepth);
            }
            for (const std::unique_ptr<core::ConditionalBranch>& branch :
                 ifStatement.ElseIfBranches()) {
                CollectPositions(branch->Body(), loopDepth);
                if (branch->Also() != nullptr) {
                    CollectPositions(branch->Also()->Body(), loopDepth);
                }
            }
            if (ifStatement.ElseBody() != nullptr) {
                CollectPositions(*ifStatement.ElseBody(), loopDepth);
            }
            continue;
        }
        if (statement->Kind() == core::AstNodeKind::WhileStatement) {
            const core::WhileStatement& whileStatement =
                static_cast<const core::WhileStatement&>(*statement);
            CollectPositions(whileStatement.Body(), loopDepth + 1);
            if (whileStatement.Also() != nullptr) {
                CollectPositions(whileStatement.Also()->Body(), loopDepth + 1);
            }
        }
    }
}

void SemanticAnalyzer::AnalyzeBlock(const core::BlockStatement& block,
                                    bool createScope) {
    Scope* previousScope = _currentScope;
    if (createScope) _currentScope = &AddScope(previousScope);
    for (const std::unique_ptr<core::Statement>& statement :
         block.Statements()) {
        AnalyzeStatement(*statement);
    }
    if (createScope) _currentScope = previousScope;
}

void SemanticAnalyzer::AnalyzeBranch(const core::ConditionalBranch& branch) {
    const SemanticType condition = AnalyzeExpression(branch.Condition());
    if (!condition.IsError() && condition.Kind() != SemanticTypeKind::Bool) {
        Report(diagnostics::DiagnosticCode::InvalidCondition,
               branch.Condition().Range(), "condition must have type 'bool'");
    }
    AnalyzeBlock(branch.Body(), true);
    if (branch.Also() != nullptr) {
        const SemanticType alsoCondition =
            AnalyzeExpression(branch.Also()->Condition());
        if (!alsoCondition.IsError() &&
            alsoCondition.Kind() != SemanticTypeKind::Bool) {
            Report(diagnostics::DiagnosticCode::InvalidCondition,
                   branch.Also()->Condition().Range(),
                   "condition must have type 'bool'");
        }
        AnalyzeBlock(branch.Also()->Body(), true);
    }
}

void SemanticAnalyzer::AnalyzeStatement(const core::Statement& statement) {
    switch (statement.Kind()) {
        case core::AstNodeKind::BlockStatement: {
            const auto& block =
                static_cast<const core::BlockStatement&>(statement);
            const bool previousUnsafe = _inUnsafe;
            _inUnsafe = _inUnsafe || block.IsUnsafe();
            AnalyzeBlock(block, true);
            _inUnsafe = previousUnsafe;
            break;
        }
        case core::AstNodeKind::VariableDeclarationStatement: {
            const core::VariableDeclarationStatement& variable =
                static_cast<const core::VariableDeclarationStatement&>(
                    statement);
            const SemanticType declaredType =
                variable.HasExplicitType()
                    ? ResolveType(*variable.ExplicitType())
                    : SemanticType::Error();
            SemanticType initializerType = SemanticType::Error();
            if (variable.Initializer() != nullptr) {
                initializerType =
                    AnalyzeExpression(*variable.Initializer(), declaredType);
            }
            SemanticType variableType = initializerType.WithoutConst();
            if (variable.HasExplicitType()) {
                variableType = declaredType;
                if (variableType.Kind() == SemanticTypeKind::Void) {
                    Report(diagnostics::DiagnosticCode::InvalidVariableType,
                           variable.ExplicitType()->Range(),
                           "a variable cannot have type 'void'");
                } else if (variable.Initializer() != nullptr &&
                           !IsAssignable(variableType, initializerType)) {
                    Report(diagnostics::DiagnosticCode::IncompatibleType,
                           variable.Initializer()->Range(),
                           TypeMismatch(variableType, initializerType));
                }
            } else if (initializerType.IsError() ||
                       initializerType.Kind() == SemanticTypeKind::Void ||
                       initializerType.Kind() == SemanticTypeKind::Function) {
                Report(diagnostics::DiagnosticCode::CannotInferType,
                       variable.NameRange(),
                       "cannot infer a variable type for '" + variable.Name() +
                           "'");
                variableType = SemanticType::Error();
            }
            if (variableType.Kind() == SemanticTypeKind::Reference &&
                variable.Initializer() == nullptr) {
                Report(
                    diagnostics::DiagnosticCode::ReferenceRequiresInitializer,
                    variable.NameRange(),
                    "a reference requires an initializer");
            }
            if (variableType.IsConst() && variable.Initializer() == nullptr &&
                variableType.Kind() != SemanticTypeKind::Reference) {
                Report(diagnostics::DiagnosticCode::ConstRequiresInitializer,
                       variable.NameRange(),
                       "a const declaration requires an initializer");
            }
            const Symbol* symbol = AddSymbol(
                std::make_unique<VariableSymbol>(
                    SymbolKind::LocalVariable, variable.Name(), variableType,
                    variable.NameRange(), !variableType.IsConst()),
                variable);
            if (symbol != nullptr && variable.Initializer() != nullptr &&
                variableType.Kind() == SemanticTypeKind::StringView &&
                BorrowsLocalString(*variable.Initializer())) {
                _localStringBorrows.insert(symbol);
            }
            if (symbol != nullptr && variable.Initializer() == nullptr &&
                variableType.Kind() == SemanticTypeKind::StringView) {
                Report(
                    diagnostics::DiagnosticCode::BorrowRequiresInitializer,
                    variable.NameRange(),
                    "a borrowed 'stringv' declaration requires an initializer");
            }
            break;
        }
        case core::AstNodeKind::ExpressionStatement:
            AnalyzeExpression(
                static_cast<const core::ExpressionStatement&>(statement)
                    .Value());
            break;
        case core::AstNodeKind::ReturnStatement: {
            const core::ReturnStatement& returnStatement =
                static_cast<const core::ReturnStatement&>(statement);
            const SemanticType expected = _currentFunction == nullptr
                                              ? SemanticType::Error()
                                              : _currentFunction->ReturnType();
            const SemanticType actual =
                returnStatement.Value() == nullptr
                    ? SemanticType(SemanticTypeKind::Void)
                    : AnalyzeExpression(*returnStatement.Value(), expected);
            if (!IsAssignable(expected, actual)) {
                const source::SourceRange range =
                    returnStatement.Value() == nullptr
                        ? returnStatement.Range()
                        : returnStatement.Value()->Range();
                Report(diagnostics::DiagnosticCode::InvalidReturnType, range,
                       TypeMismatch(expected, actual));
            } else if (returnStatement.Value() != nullptr &&
                       expected.Kind() == SemanticTypeKind::StringView &&
                       BorrowsLocalString(*returnStatement.Value())) {
                Report(diagnostics::DiagnosticCode::BorrowEscapesOwner,
                       returnStatement.Value()->Range(),
                       "borrowed string view outlives its local owning string");
            }
            break;
        }
        case core::AstNodeKind::IfStatement: {
            const core::IfStatement& ifStatement =
                static_cast<const core::IfStatement&>(statement);
            AnalyzeBranch(ifStatement.PrimaryBranch());
            for (const std::unique_ptr<core::ConditionalBranch>& branch :
                 ifStatement.ElseIfBranches()) {
                AnalyzeBranch(*branch);
            }
            if (ifStatement.ElseBody() != nullptr) {
                AnalyzeBlock(*ifStatement.ElseBody(), true);
            }
            break;
        }
        case core::AstNodeKind::RangeStatement: {
            const auto& range =
                static_cast<const core::RangeStatement&>(statement);
            AnalyzeBlock(range.Expansion(), true);
            const SemanticType source =
                _model.TypeOf(*range.Source().Initializer());
            if (!source.IsError() && source.Kind() != SemanticTypeKind::Array)
                Report(diagnostics::DiagnosticCode::IncompatibleType,
                       range.Source().Range(),
                       "range requires a fixed array value");
            if (range.IndexBinding() != nullptr) {
                const Symbol* symbol =
                    _model.DeclaredSymbol(*range.IndexBinding());
                if (symbol != nullptr && !symbol->Type().IsError()) {
                    const SemanticType index = symbol->Type();
                    if (!index.IsInteger()) {
                        Report(diagnostics::DiagnosticCode::IncompatibleType,
                               range.IndexBinding()->NameRange(),
                               "range index must have an integer type");
                    } else if (source.Kind() == SemanticTypeKind::Array &&
                               source.Length() != 0) {
                        // Reuse literal representability checks for the
                        // greatest exposed index. The hidden usize cursor
                        // cannot wrap.
                        const core::LiteralExpression last(
                            range.IndexBinding()->NameRange(),
                            TokenType::NumberLiteral,
                            std::to_string(source.Length() - 1));
                        AnalyzeLiteral(last, index);
                    }
                }
            }
            break;
        }
        case core::AstNodeKind::WhileStatement: {
            const core::WhileStatement& whileStatement =
                static_cast<const core::WhileStatement&>(statement);
            const SemanticType condition =
                AnalyzeExpression(whileStatement.Condition());
            if (!condition.IsError() &&
                condition.Kind() != SemanticTypeKind::Bool) {
                Report(diagnostics::DiagnosticCode::InvalidCondition,
                       whileStatement.Condition().Range(),
                       "condition must have type 'bool'");
            }
            ++_loopDepth;
            AnalyzeBlock(whileStatement.Body(), true);
            if (whileStatement.Update() != nullptr)
                AnalyzeExpression(*whileStatement.Update());
            if (whileStatement.Also() != nullptr) {
                const SemanticType alsoCondition =
                    AnalyzeExpression(whileStatement.Also()->Condition());
                if (!alsoCondition.IsError() &&
                    alsoCondition.Kind() != SemanticTypeKind::Bool) {
                    Report(diagnostics::DiagnosticCode::InvalidCondition,
                           whileStatement.Also()->Condition().Range(),
                           "condition must have type 'bool'");
                }
                AnalyzeBlock(whileStatement.Also()->Body(), true);
            }
            --_loopDepth;
            break;
        }
        case core::AstNodeKind::ProceedStatement:
        case core::AstNodeKind::BreakStatement:
            if (_loopDepth == 0) {
                Report(diagnostics::DiagnosticCode::LoopControlOutsideLoop,
                       statement.Range(),
                       statement.Kind() == core::AstNodeKind::ProceedStatement
                           ? "'proceed' is only valid inside a loop"
                           : "'break' is only valid inside a loop");
            }
            break;
        case core::AstNodeKind::JumpToStatement: {
            const core::JumpToStatement& jump =
                static_cast<const core::JumpToStatement&>(statement);
            if (_loopDepth == 0) {
                Report(diagnostics::DiagnosticCode::LoopControlOutsideLoop,
                       statement.Range(),
                       "'jumpto' is only valid inside a loop");
            }
            if (!_positions.contains(jump.Position())) {
                Report(diagnostics::DiagnosticCode::UnknownPosition,
                       jump.PositionRange(),
                       "unknown position '" + jump.Position() + "'");
            }
            break;
        }
        case core::AstNodeKind::PositionStatement:
            break;
        default:
            break;
    }
}

SemanticType SemanticAnalyzer::AnalyzeExpression(
    const core::Expression& expression, SemanticType expected) {
    SemanticType type = SemanticType::Error();
    switch (expression.Kind()) {
        case core::AstNodeKind::ArrayExpression:
            type = AnalyzeArray(
                static_cast<const core::ArrayExpression&>(expression),
                expected);
            break;
        case core::AstNodeKind::LiteralExpression:
            type = AnalyzeLiteral(
                static_cast<const core::LiteralExpression&>(expression),
                expected);
            break;
        case core::AstNodeKind::NameExpression:
            type = AnalyzeName(
                static_cast<const core::NameExpression&>(expression));
            break;
        case core::AstNodeKind::ParenthesizedExpression:
            type = AnalyzeExpression(
                static_cast<const core::ParenthesizedExpression&>(expression)
                    .Inner(),
                expected);
            break;
        case core::AstNodeKind::UnaryExpression:
            type = AnalyzeUnary(
                static_cast<const core::UnaryExpression&>(expression),
                expected);
            break;
        case core::AstNodeKind::BinaryExpression:
            type = AnalyzeBinary(
                static_cast<const core::BinaryExpression&>(expression),
                expected);
            break;
        case core::AstNodeKind::AssignmentExpression:
            type = AnalyzeAssignment(
                static_cast<const core::AssignmentExpression&>(expression));
            break;
        case core::AstNodeKind::CallExpression:
            type = AnalyzeCall(
                static_cast<const core::CallExpression&>(expression));
            break;
        case core::AstNodeKind::MemberCallExpression:
            type = AnalyzeMemberCall(
                static_cast<const core::MemberCallExpression&>(expression));
            break;
        case core::AstNodeKind::IndexExpression:
            type = AnalyzeIndex(
                static_cast<const core::IndexExpression&>(expression));
            break;
        case core::AstNodeKind::CastExpression:
            type = AnalyzeCast(
                static_cast<const core::CastExpression&>(expression));
            break;
        default:
            break;
    }
    _model._types.insert_or_assign(&expression, type);
    return type;
}

void SemanticAnalyzer::ValidateArrayType(SemanticType type,
                                         source::SourceRange range) {
    if (!IsArrayElement(*type.Target())) {
        Report(diagnostics::DiagnosticCode::InvalidArrayElement, range,
               "array elements must be unqualified scalars, raw pointers, or "
               "arrays of those types; use const [N]T for a read-only array");
    } else if (!StorageSize(type)) {
        Report(diagnostics::DiagnosticCode::InvalidArrayLength, range,
               "array layout exceeds the supported native size");
    }
}

SemanticType SemanticAnalyzer::AnalyzeArray(
    const core::ArrayExpression& expression, SemanticType expected) {
    SemanticType element = expected.Kind() == SemanticTypeKind::Array
                               ? *expected.Target()
                               : SemanticType::Error();
    if (expected.Kind() == SemanticTypeKind::Array &&
        expression.Length() != expected.Length())
        Report(diagnostics::DiagnosticCode::ArrayInitializerCount,
               expression.Range(),
               "expected " + std::to_string(expected.Length()) +
                   " array elements, found " +
                   std::to_string(expression.Length()));
    for (const std::unique_ptr<core::Expression>& item :
         expression.Elements()) {
        const SemanticType actual = AnalyzeExpression(*item, element);
        if (element.IsError())
            element = actual.WithoutConst();
        else if (!IsAssignable(element, actual))
            Report(diagnostics::DiagnosticCode::IncompatibleType, item->Range(),
                   TypeMismatch(element, actual));
    }
    if (element.IsError()) {
        Report(diagnostics::DiagnosticCode::CannotInferType, expression.Range(),
               "empty array initializer requires an expected array type");
        return SemanticType::Error();
    }
    const SemanticType result(SemanticTypeKind::Array, element, false,
                              expression.Length());
    ValidateArrayType(result, expression.Range());
    return result;
}

SemanticType SemanticAnalyzer::AnalyzeLiteral(
    const core::LiteralExpression& expression, SemanticType expected,
    bool negative) {
    if (expression.LiteralType() == TokenType::NumberLiteral &&
        expected.IsInteger()) {
        const SemanticTypeKind kind = expected.Kind();
        const bool isSigned = kind <= SemanticTypeKind::ISize;
        unsigned bits = 64;
        if (kind == SemanticTypeKind::I8 || kind == SemanticTypeKind::U8)
            bits = 8;
        if (kind == SemanticTypeKind::I16 || kind == SemanticTypeKind::U16)
            bits = 16;
        if (kind == SemanticTypeKind::I32 || kind == SemanticTypeKind::U32)
            bits = 32;
        if (kind == SemanticTypeKind::ISize || kind == SemanticTypeKind::USize)
            bits = sizeof(void*) * 8;
        std::uint64_t limit = std::numeric_limits<std::uint64_t>::max();
        if (isSigned)
            limit = (std::uint64_t{1} << (bits - 1)) - (negative ? 0 : 1);
        else if (bits < 64)
            limit = (std::uint64_t{1} << bits) - 1;
        std::uint64_t value = 0;
        const std::string& text = expression.Value();
        const auto parsed =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != text.data() + text.size() || value > limit ||
            (negative && !isSigned && value != 0)) {
            Report(diagnostics::DiagnosticCode::IntegerLiteralOutOfRange,
                   expression.Range(),
                   "integer literal is not representable as '" +
                       expected.Name() + "'");
        }
        return expected.WithoutConst();
    }
    switch (expression.LiteralType()) {
        case TokenType::NumberLiteral:
            return SemanticType(SemanticTypeKind::I32);
        case TokenType::FloatLiteral:
            return SemanticType(SemanticTypeKind::F64);
        case TokenType::True:
        case TokenType::False:
            return SemanticType(SemanticTypeKind::Bool);
        case TokenType::CharLiteral:
            return SemanticType(SemanticTypeKind::Char);
        case TokenType::StringLiteral:
            return SemanticType(SemanticTypeKind::StringView);
        case TokenType::Null:
            return SemanticType(SemanticTypeKind::Null);
        case TokenType::None:
            return SemanticType(SemanticTypeKind::None);
        default:
            return SemanticType::Error();
    }
}

SemanticType SemanticAnalyzer::AnalyzeCast(
    const core::CastExpression& expression) {
    const SemanticType source = AnalyzeExpression(expression.Value());
    const SemanticType target = ResolveType(expression.TargetType());
    if (source.IsError() || target.IsError()) return SemanticType::Error();
    if (!CanExplicitlyCast(source, target)) {
        Report(diagnostics::DiagnosticCode::InvalidCast, expression.Range(),
               "cannot cast '" + std::string(source.Name()) + "' to '" +
                   std::string(target.Name()) + "'");
        return SemanticType::Error();
    }
    return target.WithoutConst();
}

SemanticType SemanticAnalyzer::AnalyzeName(
    const core::NameExpression& expression) {
    const Symbol* symbol = _currentScope->Lookup(expression.Name());
    if (symbol == nullptr) {
        Report(diagnostics::DiagnosticCode::UnknownName, expression.Range(),
               "unknown name '" + expression.Name() + "'");
        return SemanticType::Error();
    }
    _model._bindings.emplace(&expression, symbol);
    return symbol->Type().WithoutConst();
}

void SemanticAnalyzer::RequireUnsafe(source::SourceRange range) {
    if (!_inUnsafe) {
        Report(diagnostics::DiagnosticCode::UnsafeOperation, range,
               "operation requires an unsafe function or unsafe block");
    }
}

bool SemanticAnalyzer::IsSafePlace(const core::Expression& expression) const {
    if (expression.Kind() == core::AstNodeKind::ParenthesizedExpression)
        return IsSafePlace(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    if (expression.Kind() == core::AstNodeKind::NameExpression)
        return !PlaceType(expression).IsError();
    if (expression.Kind() == core::AstNodeKind::IndexExpression)
        return !PlaceType(expression).IsError() &&
               IsSafePlace(static_cast<const core::IndexExpression&>(expression)
                               .Receiver());
    if (expression.Kind() == core::AstNodeKind::UnaryExpression) {
        const auto& unary =
            static_cast<const core::UnaryExpression&>(expression);
        return unary.OperatorType() == TokenType::Mul &&
               _model.TypeOf(unary.Operand()).Kind() ==
                   SemanticTypeKind::Reference;
    }
    return false;
}

bool SemanticAnalyzer::IsReferencePlace(
    const core::Expression& expression) const {
    if (expression.Kind() == core::AstNodeKind::ParenthesizedExpression)
        return IsReferencePlace(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    if (expression.Kind() == core::AstNodeKind::IndexExpression)
        return IsReferencePlace(
            static_cast<const core::IndexExpression&>(expression).Receiver());
    if (expression.Kind() == core::AstNodeKind::UnaryExpression) {
        const auto& unary =
            static_cast<const core::UnaryExpression&>(expression);
        return unary.OperatorType() == TokenType::Mul &&
               _model.TypeOf(unary.Operand()).Kind() ==
                   SemanticTypeKind::Reference;
    }
    return false;
}

SemanticType SemanticAnalyzer::PlaceType(
    const core::Expression& expression) const {
    if (expression.Kind() == core::AstNodeKind::ParenthesizedExpression) {
        return PlaceType(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    }
    if (expression.Kind() == core::AstNodeKind::NameExpression) {
        const Symbol* symbol = _model.SymbolFor(
            static_cast<const core::NameExpression&>(expression));
        return symbol == nullptr ? SemanticType::Error() : symbol->Type();
    }
    if (expression.Kind() == core::AstNodeKind::IndexExpression) {
        const auto& index =
            static_cast<const core::IndexExpression&>(expression);
        const SemanticType receiver = PlaceType(index.Receiver());
        if (receiver.Kind() == SemanticTypeKind::Array)
            return receiver.IsConst() ? receiver.Target()->WithConst()
                                      : *receiver.Target();
        return SemanticType::Error();
    }
    if (expression.Kind() == core::AstNodeKind::UnaryExpression) {
        const auto& unary =
            static_cast<const core::UnaryExpression&>(expression);
        const SemanticType pointer = _model.TypeOf(unary.Operand());
        if (unary.OperatorType() == TokenType::Mul &&
            (pointer.Kind() == SemanticTypeKind::Pointer ||
             pointer.Kind() == SemanticTypeKind::Reference))
            return *pointer.Target();
    }
    return SemanticType::Error();
}

SemanticType SemanticAnalyzer::AnalyzeUnary(
    const core::UnaryExpression& expression, SemanticType expected) {
    const TokenType operation = expression.OperatorType();
    SemanticType operand = SemanticType::Error();
    const core::Expression* literal = &expression.Operand();
    while (literal->Kind() == core::AstNodeKind::ParenthesizedExpression)
        literal = &static_cast<const core::ParenthesizedExpression&>(*literal)
                       .Inner();
    if ((operation == TokenType::Sub || operation == TokenType::Add) &&
        expected.IsInteger() &&
        literal->Kind() == core::AstNodeKind::LiteralExpression) {
        operand = AnalyzeLiteral(
            static_cast<const core::LiteralExpression&>(*literal), expected,
            operation == TokenType::Sub);
        const core::Expression* layer = &expression.Operand();
        for (;;) {
            _model._types.insert_or_assign(layer, operand);
            if (layer == literal) break;
            layer = &static_cast<const core::ParenthesizedExpression&>(*layer)
                         .Inner();
        }
    } else {
        operand = AnalyzeExpression(
            expression.Operand(),
            operation == TokenType::Sub || operation == TokenType::Add
                ? expected
                : SemanticType::Error());
    }
    if (operand.IsError()) return operand;
    if (operation == TokenType::Increment) {
        const SemanticType place = PlaceType(expression.Operand());
        if (place.IsError()) {
            Report(diagnostics::DiagnosticCode::SemanticInvalidAssignmentTarget,
                   expression.Operand().Range(),
                   "increment requires a storage location");
            return SemanticType::Error();
        }
        if (place.IsConst()) {
            Report(diagnostics::DiagnosticCode::SymbolNotWritable,
                   expression.Operand().Range(),
                   "increment target is not writable");
            return SemanticType::Error();
        }
        if (!place.IsInteger()) {
            Report(diagnostics::DiagnosticCode::IncompatibleType,
                   expression.Range(),
                   "increment requires an integer storage location");
            return SemanticType::Error();
        }
        return place.WithoutConst();
    }
    if (operation == TokenType::Ampersand) {
        SemanticType target = PlaceType(expression.Operand());
        if (target.IsError() ||
            !(target.IsNumeric() || target.Kind() == SemanticTypeKind::Bool ||
              target.Kind() == SemanticTypeKind::Char ||
              target.Kind() == SemanticTypeKind::Pointer ||
              target.Kind() == SemanticTypeKind::Array) ||
            !IsSafePlace(expression.Operand())) {
            Report(diagnostics::DiagnosticCode::InvalidReference,
                   expression.Range(),
                   "safe borrowing requires scalar, array, or raw-pointer "
                   "storage owned by a variable or reference");
            return SemanticType::Error();
        }
        if (expected.Kind() == SemanticTypeKind::Reference &&
            expected.Target()->IsConst())
            target = target.WithConst();
        return SemanticType(SemanticTypeKind::Reference, target);
    }
    if (operation == TokenType::RawAddress) {
        RequireUnsafe(expression.Range());
        const SemanticType target = PlaceType(expression.Operand());
        if (target.IsError() || target.Kind() == SemanticTypeKind::Function ||
            target.Kind() == SemanticTypeKind::Void) {
            Report(diagnostics::DiagnosticCode::InvalidPointerAccess,
                   expression.Operand().Range(),
                   "raw address requires a variable or dereferenced storage "
                   "location");
            return SemanticType::Error();
        }
        if (target.Kind() == SemanticTypeKind::Reference ||
            IsReferencePlace(expression.Operand())) {
            Report(
                diagnostics::DiagnosticCode::ReferenceEscape,
                expression.Range(),
                "raw address conversion of reference storage is not supported");
            return SemanticType::Error();
        }
        return SemanticType(SemanticTypeKind::Pointer, target);
    }
    if (operation == TokenType::Mul &&
        (operand.Kind() == SemanticTypeKind::Pointer ||
         operand.Kind() == SemanticTypeKind::Reference)) {
        if (operand.Kind() == SemanticTypeKind::Pointer)
            RequireUnsafe(expression.Range());
        if (operand.Target()->Kind() == SemanticTypeKind::Void) {
            Report(diagnostics::DiagnosticCode::InvalidPointerAccess,
                   expression.Range(), "cannot dereference a void pointer");
            return SemanticType::Error();
        }
        return operand.Target()->WithoutConst();
    }
    if ((operation == TokenType::Add || operation == TokenType::Sub) &&
        operand.IsNumeric()) {
        return operand;
    }
    if (operation == TokenType::Bang &&
        operand.Kind() == SemanticTypeKind::Bool) {
        return operand;
    }
    Report(diagnostics::DiagnosticCode::InvalidOperand, expression.Range(),
           "operator is not defined for type '" + std::string(operand.Name()) +
               "'");
    return SemanticType::Error();
}

SemanticType SemanticAnalyzer::AnalyzeBinary(
    const core::BinaryExpression& expression, SemanticType expected) {
    SemanticType left = AnalyzeExpression(
        expression.Left(),
        expected.IsInteger() ? expected : SemanticType::Error());
    const SemanticType right = AnalyzeExpression(
        expression.Right(), left.IsInteger() ? left : SemanticType::Error());
    if (right.IsInteger() && !expected.IsInteger() &&
        expression.Left().Kind() == core::AstNodeKind::LiteralExpression)
        left = AnalyzeExpression(expression.Left(), right);
    if (left.Kind() == SemanticTypeKind::Reference ||
        right.Kind() == SemanticTypeKind::Reference ||
        left.Kind() == SemanticTypeKind::Array ||
        right.Kind() == SemanticTypeKind::Array) {
        Report(diagnostics::DiagnosticCode::InvalidOperand, expression.Range(),
               "arrays and reference handles do not support binary operators");
        return SemanticType::Error();
    }
    if (left.IsError() || right.IsError()) return SemanticType::Error();
    const TokenType operation = expression.OperatorType();
    const bool hasPointer = left.Kind() == SemanticTypeKind::Pointer ||
                            right.Kind() == SemanticTypeKind::Pointer;
    if (hasPointer || left.Kind() == SemanticTypeKind::Null ||
        right.Kind() == SemanticTypeKind::Null) {
        const bool equality = operation == TokenType::Equal ||
                              operation == TokenType::NotEqual ||
                              operation == TokenType::StrictEqual ||
                              operation == TokenType::StrictNotEqual;
        const bool leftZero = left.Kind() == SemanticTypeKind::Null ||
                              left.Kind() == SemanticTypeKind::None;
        const bool rightZero = right.Kind() == SemanticTypeKind::Null ||
                               right.Kind() == SemanticTypeKind::None;
        const bool strict = operation == TokenType::StrictEqual ||
                            operation == TokenType::StrictNotEqual;
        if (equality &&
            (strict ? left == right
                    : ((leftZero && rightZero) ||
                       (hasPointer &&
                        (leftZero || rightZero || IsAssignable(left, right) ||
                         IsAssignable(right, left)))))) {
            return SemanticType(SemanticTypeKind::Bool);
        }
        Report(diagnostics::DiagnosticCode::InvalidOperand, expression.Range(),
               "operator is not defined for pointer operands of these types");
        return SemanticType::Error();
    }
    const bool same = left == right;
    const bool leftString = left.Kind() == SemanticTypeKind::String ||
                            left.Kind() == SemanticTypeKind::StringView;
    const bool rightString = right.Kind() == SemanticTypeKind::String ||
                             right.Kind() == SemanticTypeKind::StringView;
    if (operation == TokenType::Add && leftString && rightString) {
        return SemanticType(SemanticTypeKind::String);
    }
    if (operation == TokenType::Add || operation == TokenType::Sub ||
        operation == TokenType::Mul || operation == TokenType::Div ||
        operation == TokenType::Mod) {
        if (same && left.IsNumeric() &&
            !(operation == TokenType::Mod && left.IsFloating())) {
            return left;
        }
    } else if (operation == TokenType::And || operation == TokenType::Or ||
               operation == TokenType::Xor) {
        if (same &&
            (left.Kind() == SemanticTypeKind::Bool || left.IsInteger())) {
            return left;
        }
    } else if (operation == TokenType::Less ||
               operation == TokenType::LessEqual ||
               operation == TokenType::Greater ||
               operation == TokenType::GreaterEqual) {
        if (same && left.IsNumeric()) {
            return SemanticType(SemanticTypeKind::Bool);
        }
    } else if (operation == TokenType::Equal ||
               operation == TokenType::NotEqual) {
        if ((leftString && rightString) || same ||
            left.Kind() == SemanticTypeKind::None ||
            right.Kind() == SemanticTypeKind::None) {
            return SemanticType(SemanticTypeKind::Bool);
        }
    } else if (operation == TokenType::StrictEqual ||
               operation == TokenType::StrictNotEqual) {
        return SemanticType(SemanticTypeKind::Bool);
    }
    Report(diagnostics::DiagnosticCode::InvalidOperand, expression.Range(),
           "operator is not defined for types '" + std::string(left.Name()) +
               "' and '" + std::string(right.Name()) + "'");
    return SemanticType::Error();
}

SemanticType SemanticAnalyzer::AnalyzeAssignment(
    const core::AssignmentExpression& expression) {
    const SemanticType targetValue = AnalyzeExpression(expression.Target());
    const SemanticType target = PlaceType(expression.Target());
    const SemanticType value = AnalyzeExpression(expression.Value(), target);
    if (targetValue.IsError()) return SemanticType::Error();
    if (target.IsError()) {
        Report(diagnostics::DiagnosticCode::SemanticInvalidAssignmentTarget,
               expression.Target().Range(),
               "expression is not a storage location");
        return SemanticType::Error();
    }
    const core::Expression* place = &expression.Target();
    while (place->Kind() == core::AstNodeKind::ParenthesizedExpression) {
        place =
            &static_cast<const core::ParenthesizedExpression&>(*place).Inner();
    }
    const Symbol* symbol =
        place->Kind() == core::AstNodeKind::NameExpression
            ? _model.SymbolFor(static_cast<const core::NameExpression&>(*place))
            : nullptr;
    if (target.IsConst() || (symbol != nullptr && !symbol->Writable())) {
        Report(diagnostics::DiagnosticCode::SymbolNotWritable,
               expression.Target().Range(),
               symbol != nullptr ? "'" + symbol->Name() + "' is not writable"
                                 : "assignment target is not writable");
        return SemanticType::Error();
    }
    if (!IsAssignable(target, value)) {
        Report(diagnostics::DiagnosticCode::IncompatibleType,
               expression.Value().Range(), TypeMismatch(target, value));
        return SemanticType::Error();
    }
    if (symbol != nullptr && target.Kind() == SemanticTypeKind::StringView) {
        if (BorrowsLocalString(expression.Value())) {
            _localStringBorrows.insert(symbol);
        } else {
            _localStringBorrows.erase(symbol);
        }
    }
    return target.WithoutConst();
}

bool SemanticAnalyzer::BorrowsLocalString(
    const core::Expression& expression) const {
    switch (expression.Kind()) {
        case core::AstNodeKind::NameExpression: {
            const Symbol* symbol = _model.SymbolFor(
                static_cast<const core::NameExpression&>(expression));
            if (symbol == nullptr) return false;
            if (_localStringBorrows.contains(symbol)) return true;
            return symbol->Type().Kind() == SemanticTypeKind::String;
        }
        case core::AstNodeKind::ParenthesizedExpression:
            return BorrowsLocalString(
                static_cast<const core::ParenthesizedExpression&>(expression)
                    .Inner());
        case core::AstNodeKind::CastExpression: {
            const core::CastExpression& cast =
                static_cast<const core::CastExpression&>(expression);
            return SemanticType::FromSyntax(cast.TargetType()).Kind() ==
                       SemanticTypeKind::StringView &&
                   (_model.TypeOf(cast.Value()).Kind() ==
                        SemanticTypeKind::String ||
                    BorrowsLocalString(cast.Value()));
        }
        case core::AstNodeKind::MemberCallExpression: {
            const core::MemberCallExpression& call =
                static_cast<const core::MemberCallExpression&>(expression);
            if (call.Member() != "view" && call.Member() != "substr") {
                return false;
            }
            return _model.TypeOf(call.Receiver()).Kind() ==
                       SemanticTypeKind::String ||
                   BorrowsLocalString(call.Receiver());
        }
        default:
            return false;
    }
}

SemanticType SemanticAnalyzer::AnalyzeCall(
    const core::CallExpression& expression) {
    const SemanticType calleeType = AnalyzeExpression(expression.Callee());
    const FunctionSymbol* function = nullptr;
    if (expression.Callee().Kind() == core::AstNodeKind::NameExpression) {
        const Symbol* symbol = _model.SymbolFor(
            static_cast<const core::NameExpression&>(expression.Callee()));
        if (symbol != nullptr && symbol->Kind() == SymbolKind::Function) {
            function = static_cast<const FunctionSymbol*>(symbol);
        }
    }
    std::vector<SemanticType> argumentTypes;
    for (std::size_t index = 0; index < expression.Arguments().size();
         ++index) {
        const SemanticType expected =
            function != nullptr &&
                    function->BuiltIn() == BuiltInFunction::None &&
                    index < function->ParameterTypes().size()
                ? function->ParameterTypes()[index]
                : SemanticType::Error();
        argumentTypes.push_back(
            AnalyzeExpression(*expression.Arguments()[index], expected));
    }
    if (calleeType.IsError()) return SemanticType::Error();
    if (function == nullptr) {
        Report(diagnostics::DiagnosticCode::NotCallable,
               expression.Callee().Range(), "expression is not callable");
        return SemanticType::Error();
    }
    if (function->IsUnsafe()) RequireUnsafe(expression.Callee().Range());
    if (function->BuiltIn() == BuiltInFunction::Stdout ||
        function->BuiltIn() == BuiltInFunction::StdoutLine) {
        if (argumentTypes.size() != 1) {
            Report(diagnostics::DiagnosticCode::WrongArgumentCount,
                   expression.Range(),
                   "function '" + function->Name() +
                       "' expects 1 argument, found " +
                       std::to_string(argumentTypes.size()));
        } else if (!argumentTypes.front().IsError() &&
                   !argumentTypes.front().IsPrintable()) {
            Report(diagnostics::DiagnosticCode::IncompatibleArgument,
                   expression.Arguments().front()->Range(),
                   "function '" + function->Name() +
                       "' requires a printable value, found '" +
                       std::string(argumentTypes.front().Name()) + "'");
        }
        return function->ReturnType().WithoutConst();
    }
    if (argumentTypes.size() != function->ParameterTypes().size()) {
        Report(
            diagnostics::DiagnosticCode::WrongArgumentCount, expression.Range(),
            "function '" + function->Name() + "' expects " +
                std::to_string(function->ParameterTypes().size()) +
                " argument(s), found " + std::to_string(argumentTypes.size()));
    }
    const std::size_t count =
        argumentTypes.size() < function->ParameterTypes().size()
            ? argumentTypes.size()
            : function->ParameterTypes().size();
    for (std::size_t index = 0; index < count; ++index) {
        if (!IsAssignable(function->ParameterTypes()[index],
                          argumentTypes[index]) &&
            !CanReborrow(function->ParameterTypes()[index],
                         argumentTypes[index])) {
            Report(diagnostics::DiagnosticCode::IncompatibleArgument,
                   expression.Arguments()[index]->Range(),
                   "argument " + std::to_string(index + 1) + ": " +
                       TypeMismatch(function->ParameterTypes()[index],
                                    argumentTypes[index]));
        }
    }
    return function->ReturnType().WithoutConst();
}

namespace {

bool IsStringViewIndex(SemanticType type) {
    return type.Kind() == SemanticTypeKind::I32 ||
           type.Kind() == SemanticTypeKind::U32;
}

}  // namespace

SemanticType SemanticAnalyzer::AnalyzeMemberCall(
    const core::MemberCallExpression& expression) {
    const SemanticType receiver = AnalyzeExpression(expression.Receiver());
    std::vector<SemanticType> arguments;
    arguments.reserve(expression.Arguments().size());
    for (const std::unique_ptr<core::Expression>& argument :
         expression.Arguments()) {
        arguments.push_back(AnalyzeExpression(*argument));
    }
    if (receiver.IsError()) return SemanticType::Error();
    if (receiver.Kind() == SemanticTypeKind::Array &&
        expression.Member() == "len") {
        if (!arguments.empty())
            Report(diagnostics::DiagnosticCode::WrongArgumentCount,
                   expression.Range(), "array len expects no arguments");
        return SemanticType(SemanticTypeKind::USize);
    }
    const bool isView = receiver.Kind() == SemanticTypeKind::StringView;
    const bool isString = receiver.Kind() == SemanticTypeKind::String;
    const bool scalarToString = receiver.IsNumeric() ||
                                receiver.Kind() == SemanticTypeKind::Bool ||
                                receiver.Kind() == SemanticTypeKind::Char;
    if (!isView && !isString &&
        !(scalarToString && expression.Member() == "string")) {
        Report(diagnostics::DiagnosticCode::InvalidMember,
               expression.MemberRange(),
               "type '" + std::string(receiver.Name()) + "' has no member '" +
                   expression.Member() + "'");
        return SemanticType::Error();
    }

    std::size_t expectedCount = 0;
    SemanticType result = SemanticType::Error();
    if (expression.Member() == "string") {
        result = SemanticType(SemanticTypeKind::String);
    } else if (expression.Member() == "view" && isString) {
        result = SemanticType(SemanticTypeKind::StringView);
    } else if (expression.Member() == "copy" && isString) {
        result = SemanticType(SemanticTypeKind::String);
    } else if (expression.Member() == "len" && (isView || isString)) {
        result = SemanticType(SemanticTypeKind::U32);
    } else if (expression.Member() == "at" && (isView || isString)) {
        expectedCount = 1;
        result = SemanticType(SemanticTypeKind::Char);
    } else if (expression.Member() == "substr" && (isView || isString)) {
        expectedCount = 2;
        result = SemanticType(SemanticTypeKind::StringView);
    } else {
        Report(diagnostics::DiagnosticCode::InvalidMember,
               expression.MemberRange(),
               "type '" + std::string(receiver.Name()) + "' has no member '" +
                   expression.Member() + "'");
        return SemanticType::Error();
    }

    if (arguments.size() != expectedCount) {
        Report(diagnostics::DiagnosticCode::WrongArgumentCount,
               expression.Range(),
               "member '" + expression.Member() + "' expects " +
                   std::to_string(expectedCount) + " argument(s), found " +
                   std::to_string(arguments.size()));
        return result;
    }
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (!arguments[index].IsError() &&
            !IsStringViewIndex(arguments[index])) {
            Report(diagnostics::DiagnosticCode::IncompatibleArgument,
                   expression.Arguments()[index]->Range(),
                   "string index must have type 'i32' or 'u32', found '" +
                       std::string(arguments[index].Name()) + "'");
        }
    }
    return result;
}

SemanticType SemanticAnalyzer::AnalyzeIndex(
    const core::IndexExpression& expression) {
    const SemanticType receiver = AnalyzeExpression(expression.Receiver());
    const SemanticType index = AnalyzeExpression(
        expression.Index(), receiver.Kind() == SemanticTypeKind::Array
                                ? SemanticType(SemanticTypeKind::USize)
                                : SemanticType::Error());
    if (receiver.Kind() == SemanticTypeKind::Array) {
        if (!index.IsError() && index.Kind() != SemanticTypeKind::USize)
            Report(diagnostics::DiagnosticCode::IncompatibleArgument,
                   expression.Index().Range(),
                   "array index must have type 'usize'");
        const std::optional<std::uint64_t> constant =
            ConstantIndex(expression.Index());
        if (!expression.IsRangeElement() &&
            (receiver.Length() == 0 ||
             (constant && *constant >= receiver.Length())))
            Report(diagnostics::DiagnosticCode::ArrayIndexOutOfBounds,
                   expression.Index().Range(),
                   "index is outside array length " +
                       std::to_string(receiver.Length()));
        return receiver.Target()->WithoutConst();
    }
    if (!receiver.IsError() &&
        receiver.Kind() != SemanticTypeKind::StringView &&
        receiver.Kind() != SemanticTypeKind::String) {
        Report(diagnostics::DiagnosticCode::NotIndexable,
               expression.Receiver().Range(),
               "type '" + std::string(receiver.Name()) + "' cannot be indexed");
        return SemanticType::Error();
    }
    if (!index.IsError() && !IsStringViewIndex(index)) {
        Report(diagnostics::DiagnosticCode::IncompatibleArgument,
               expression.Index().Range(),
               "string index must have type 'i32' or 'u32', found '" +
                   std::string(index.Name()) + "'");
    }
    return receiver.IsError() ? SemanticType::Error()
                              : SemanticType(SemanticTypeKind::Char);
}

}  // namespace flecha::semantic
