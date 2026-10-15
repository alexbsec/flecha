#include "semantic/semantic_analyzer.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/token_type.hpp"

namespace flecha::semantic {
namespace {

std::string TypeMismatch(SemanticType expected, SemanticType actual) {
    return "expected '" + std::string(expected.Name()) + "', found '" +
           std::string(actual.Name()) + "'";
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
            parameterTypes.push_back(
                SemanticType::FromSyntax(parameter->Type().Type()));
        }
        AddSymbol(std::make_unique<FunctionSymbol>(
                      function.Name(),
                      SemanticType::FromSyntax(function.ReturnType().Type()),
                      std::move(parameterTypes), function.NameRange()),
                  function);
    }
}

SemanticModel SemanticAnalyzer::Analyze(const core::CompilationUnit& unit) {
    _model = SemanticModel{};
    _currentScope = &AddScope(nullptr);
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
    const Symbol* functionBase = _model.DeclaredSymbol(function);
    if (functionBase != nullptr &&
        functionBase->Kind() == SymbolKind::Function) {
        _currentFunction = static_cast<const FunctionSymbol*>(functionBase);
    } else {
        _currentFunction = nullptr;
    }

    _positions.clear();
    _loopDepth = 0;
    CollectPositions(function.Body(), 0);

    Scope* previousScope = _currentScope;
    _currentScope = &AddScope(previousScope);
    for (const std::unique_ptr<core::Parameter>& parameter :
         function.Parameters()) {
        AddSymbol(std::make_unique<VariableSymbol>(
                      SymbolKind::Parameter, parameter->Name(),
                      SemanticType::FromSyntax(parameter->Type().Type()),
                      parameter->NameRange(), true),
                  *parameter);
    }
    AnalyzeBlock(function.Body(), false);
    _currentScope = previousScope;
    _currentFunction = nullptr;
}

void SemanticAnalyzer::CollectPositions(const core::BlockStatement& block,
                                        std::size_t loopDepth) {
    for (const std::unique_ptr<core::Statement>& statement :
         block.Statements()) {
        if (statement->Kind() == core::AstNodeKind::PositionStatement) {
            const core::PositionStatement& position =
                static_cast<const core::PositionStatement&>(*statement);
            if (loopDepth == 0) {
                Report(diagnostics::DiagnosticCode::LoopControlOutsideLoop,
                       position.Range(),
                       "positions are only valid inside a loop");
            }
            if (!_positions.emplace(position.Name(), position.NameRange()).second) {
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
        case core::AstNodeKind::VariableDeclarationStatement: {
            const core::VariableDeclarationStatement& variable =
                static_cast<const core::VariableDeclarationStatement&>(
                    statement);
            SemanticType initializerType = SemanticType::Error();
            if (variable.Initializer() != nullptr) {
                initializerType = AnalyzeExpression(*variable.Initializer());
            }
            SemanticType variableType = initializerType;
            if (variable.HasExplicitType()) {
                variableType =
                    SemanticType::FromSyntax(variable.ExplicitType()->Type());
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
            AddSymbol(std::make_unique<VariableSymbol>(
                          SymbolKind::LocalVariable, variable.Name(),
                          variableType, variable.NameRange(), true),
                      variable);
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
                    : AnalyzeExpression(*returnStatement.Value());
            if (!IsAssignable(expected, actual)) {
                const source::SourceRange range =
                    returnStatement.Value() == nullptr
                        ? returnStatement.Range()
                        : returnStatement.Value()->Range();
                Report(diagnostics::DiagnosticCode::InvalidReturnType, range,
                       TypeMismatch(expected, actual));
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
    const core::Expression& expression) {
    SemanticType type = SemanticType::Error();
    switch (expression.Kind()) {
        case core::AstNodeKind::LiteralExpression:
            type = AnalyzeLiteral(
                static_cast<const core::LiteralExpression&>(expression));
            break;
        case core::AstNodeKind::NameExpression:
            type = AnalyzeName(
                static_cast<const core::NameExpression&>(expression));
            break;
        case core::AstNodeKind::ParenthesizedExpression:
            type = AnalyzeExpression(
                static_cast<const core::ParenthesizedExpression&>(expression)
                    .Inner());
            break;
        case core::AstNodeKind::UnaryExpression:
            type = AnalyzeUnary(
                static_cast<const core::UnaryExpression&>(expression));
            break;
        case core::AstNodeKind::BinaryExpression:
            type = AnalyzeBinary(
                static_cast<const core::BinaryExpression&>(expression));
            break;
        case core::AstNodeKind::AssignmentExpression:
            type = AnalyzeAssignment(
                static_cast<const core::AssignmentExpression&>(expression));
            break;
        case core::AstNodeKind::CallExpression:
            type = AnalyzeCall(
                static_cast<const core::CallExpression&>(expression));
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

SemanticType SemanticAnalyzer::AnalyzeLiteral(
    const core::LiteralExpression& expression) {
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
    const SemanticType target =
        SemanticType::FromSyntax(expression.TargetType().Type());
    if (source.IsError() || target.IsError()) return SemanticType::Error();
    if (!CanExplicitlyCast(source, target)) {
        Report(diagnostics::DiagnosticCode::InvalidCast, expression.Range(),
               "cannot cast '" + std::string(source.Name()) + "' to '" +
                   std::string(target.Name()) + "'");
        return SemanticType::Error();
    }
    return target;
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
    return symbol->Type();
}

SemanticType SemanticAnalyzer::AnalyzeUnary(
    const core::UnaryExpression& expression) {
    const SemanticType operand = AnalyzeExpression(expression.Operand());
    if (operand.IsError()) return operand;
    const TokenType operation = expression.OperatorType();
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
    const core::BinaryExpression& expression) {
    const SemanticType left = AnalyzeExpression(expression.Left());
    const SemanticType right = AnalyzeExpression(expression.Right());
    if (left.IsError() || right.IsError()) return SemanticType::Error();
    const TokenType operation = expression.OperatorType();
    const bool same = left == right;
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
        if (same || left.Kind() == SemanticTypeKind::None ||
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
    const SemanticType value = AnalyzeExpression(expression.Value());
    if (expression.Target().Kind() != core::AstNodeKind::NameExpression) {
        AnalyzeExpression(expression.Target());
        Report(diagnostics::DiagnosticCode::SemanticInvalidAssignmentTarget,
               expression.Target().Range(),
               "expression is not a writable assignment target");
        return SemanticType::Error();
    }
    const core::NameExpression& name =
        static_cast<const core::NameExpression&>(expression.Target());
    const SemanticType target = AnalyzeName(name);
    _model._types.insert_or_assign(&expression.Target(), target);
    const Symbol* symbol = _model.SymbolFor(name);
    if (symbol == nullptr) return SemanticType::Error();
    if (!symbol->Writable()) {
        Report(diagnostics::DiagnosticCode::SymbolNotWritable, name.Range(),
               "'" + name.Name() + "' is not writable");
        return SemanticType::Error();
    }
    if (!IsAssignable(target, value)) {
        Report(diagnostics::DiagnosticCode::IncompatibleType,
               expression.Value().Range(), TypeMismatch(target, value));
        return SemanticType::Error();
    }
    return target;
}

SemanticType SemanticAnalyzer::AnalyzeCall(
    const core::CallExpression& expression) {
    const SemanticType calleeType = AnalyzeExpression(expression.Callee());
    std::vector<SemanticType> argumentTypes;
    argumentTypes.reserve(expression.Arguments().size());
    for (const std::unique_ptr<core::Expression>& argument :
         expression.Arguments()) {
        argumentTypes.push_back(AnalyzeExpression(*argument));
    }
    if (calleeType.IsError()) return SemanticType::Error();

    const FunctionSymbol* function = nullptr;
    if (expression.Callee().Kind() == core::AstNodeKind::NameExpression) {
        const Symbol* symbol = _model.SymbolFor(
            static_cast<const core::NameExpression&>(expression.Callee()));
        if (symbol != nullptr && symbol->Kind() == SymbolKind::Function) {
            function = static_cast<const FunctionSymbol*>(symbol);
        }
    }
    if (function == nullptr) {
        Report(diagnostics::DiagnosticCode::NotCallable,
               expression.Callee().Range(), "expression is not callable");
        return SemanticType::Error();
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
                          argumentTypes[index])) {
            Report(diagnostics::DiagnosticCode::IncompatibleArgument,
                   expression.Arguments()[index]->Range(),
                   "argument " + std::to_string(index + 1) + ": " +
                       TypeMismatch(function->ParameterTypes()[index],
                                    argumentTypes[index]));
        }
    }
    return function->ReturnType();
}

}  // namespace flecha::semantic
