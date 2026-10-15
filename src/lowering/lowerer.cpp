#include "lowering/lowerer.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace flecha::lowering {

std::unique_ptr<BoundProgram> Lowerer::Lower(
    const core::CompilationUnit& unit,
    const semantic::SemanticModel& semanticModel,
    const diagnostics::DiagnosticBag& diagnostics) {
    if (diagnostics.HasErrors()) {
        return nullptr;
    }

    _semanticModel = &semanticModel;
    std::vector<std::unique_ptr<BoundFunction>> functions;
    functions.reserve(unit.Declarations().size());
    for (const std::unique_ptr<core::Declaration>& declaration :
         unit.Declarations()) {
        if (declaration->Kind() != core::AstNodeKind::FunctionDeclaration) {
            continue;
        }
        functions.push_back(LowerFunction(
            static_cast<const core::FunctionDeclaration&>(*declaration)));
    }
    _semanticModel = nullptr;
    return std::make_unique<BoundProgram>(std::move(functions));
}

std::unique_ptr<BoundFunction> Lowerer::LowerFunction(
    const core::FunctionDeclaration& function) {
    const semantic::Symbol* symbol = _semanticModel->DeclaredSymbol(function);
    const semantic::FunctionSymbol& functionSymbol =
        static_cast<const semantic::FunctionSymbol&>(*symbol);

    std::vector<const semantic::VariableSymbol*> parameters;
    parameters.reserve(function.Parameters().size());
    for (const std::unique_ptr<core::Parameter>& parameter :
         function.Parameters()) {
        const semantic::Symbol* parameterSymbol =
            _semanticModel->DeclaredSymbol(*parameter);
        parameters.push_back(
            static_cast<const semantic::VariableSymbol*>(parameterSymbol));
    }

    return std::make_unique<BoundFunction>(
        functionSymbol, std::move(parameters), LowerBlock(function.Body()));
}

std::vector<std::unique_ptr<BoundStatement>> Lowerer::LowerStatements(
    const core::BlockStatement& block) {
    std::vector<std::unique_ptr<BoundStatement>> statements;
    statements.reserve(block.Statements().size());
    for (const std::unique_ptr<core::Statement>& statement :
         block.Statements()) {
        statements.push_back(LowerStatement(*statement));
    }
    return statements;
}

std::unique_ptr<BoundBlockStatement> Lowerer::LowerBlock(
    const core::BlockStatement& block) {
    return std::make_unique<BoundBlockStatement>(block.Range(),
                                                 LowerStatements(block));
}

std::unique_ptr<BoundStatement> Lowerer::LowerStatement(
    const core::Statement& statement) {
    switch (statement.Kind()) {
        case core::AstNodeKind::VariableDeclarationStatement: {
            const core::VariableDeclarationStatement& declaration =
                static_cast<const core::VariableDeclarationStatement&>(
                    statement);
            const semantic::VariableSymbol& symbol =
                static_cast<const semantic::VariableSymbol&>(
                    *_semanticModel->DeclaredSymbol(declaration));
            return std::make_unique<BoundVariableDeclarationStatement>(
                declaration.Range(), symbol,
                LowerExpression(*declaration.Initializer()));
        }
        case core::AstNodeKind::ExpressionStatement: {
            const core::ExpressionStatement& expressionStatement =
                static_cast<const core::ExpressionStatement&>(statement);
            return std::make_unique<BoundExpressionStatement>(
                statement.Range(),
                LowerExpression(expressionStatement.Value()));
        }
        case core::AstNodeKind::ReturnStatement: {
            const core::ReturnStatement& returnStatement =
                static_cast<const core::ReturnStatement&>(statement);
            std::unique_ptr<BoundExpression> expression;
            if (returnStatement.Value() != nullptr) {
                expression = LowerExpression(*returnStatement.Value());
            }
            return std::make_unique<BoundReturnStatement>(
                statement.Range(), std::move(expression));
        }
        case core::AstNodeKind::IfStatement:
            return LowerIf(static_cast<const core::IfStatement&>(statement));
        case core::AstNodeKind::WhileStatement:
            return LowerWhile(
                static_cast<const core::WhileStatement&>(statement));
        case core::AstNodeKind::ProceedStatement:
            return std::make_unique<BoundProceedStatement>(statement.Range());
        case core::AstNodeKind::BreakStatement:
            return std::make_unique<BoundBreakStatement>(statement.Range());
        case core::AstNodeKind::JumpToStatement: {
            const core::JumpToStatement& jump =
                static_cast<const core::JumpToStatement&>(statement);
            return std::make_unique<BoundJumpToStatement>(statement.Range(),
                                                          jump.Position());
        }
        case core::AstNodeKind::PositionStatement: {
            const core::PositionStatement& position =
                static_cast<const core::PositionStatement&>(statement);
            return std::make_unique<BoundPositionStatement>(statement.Range(),
                                                            position.Name());
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<BoundIfStatement> Lowerer::LowerBranch(
    const core::ConditionalBranch& branch,
    std::unique_ptr<BoundBlockStatement> elseBlock) {
    std::vector<std::unique_ptr<BoundStatement>> thenStatements =
        LowerStatements(branch.Body());
    if (branch.Also() != nullptr) {
        const core::AlsoClause& alsoClause = *branch.Also();
        thenStatements.push_back(std::make_unique<BoundIfStatement>(
            alsoClause.Range(), LowerExpression(alsoClause.Condition()),
            LowerBlock(alsoClause.Body()), nullptr));
    }
    const source::SourceLocation thenEnd = branch.Also() == nullptr
                                               ? branch.Body().Range().end
                                               : branch.Also()->Range().end;
    std::unique_ptr<BoundBlockStatement> thenBlock =
        std::make_unique<BoundBlockStatement>(
            source::SourceRange{branch.Body().Range().start, thenEnd},
            std::move(thenStatements));
    return std::make_unique<BoundIfStatement>(
        branch.Range(), LowerExpression(branch.Condition()),
        std::move(thenBlock), std::move(elseBlock));
}

std::unique_ptr<BoundStatement> Lowerer::LowerIf(
    const core::IfStatement& statement) {
    std::unique_ptr<BoundBlockStatement> alternative;
    if (statement.ElseBody() != nullptr) {
        alternative = LowerBlock(*statement.ElseBody());
    }

    const std::vector<std::unique_ptr<core::ConditionalBranch>>& branches =
        statement.ElseIfBranches();
    for (std::size_t index = branches.size(); index > 0; --index) {
        std::unique_ptr<BoundIfStatement> elseIf =
            LowerBranch(*branches[index - 1], std::move(alternative));
        std::vector<std::unique_ptr<BoundStatement>> statements;
        statements.push_back(std::move(elseIf));
        alternative = std::make_unique<BoundBlockStatement>(
            branches[index - 1]->Range(), std::move(statements));
    }
    return LowerBranch(statement.PrimaryBranch(), std::move(alternative));
}

std::unique_ptr<BoundStatement> Lowerer::LowerWhile(
    const core::WhileStatement& statement) {
    std::vector<std::unique_ptr<BoundStatement>> bodyStatements =
        LowerStatements(statement.Body());
    if (statement.Also() != nullptr) {
        const core::AlsoClause& alsoClause = *statement.Also();
        bodyStatements.push_back(std::make_unique<BoundIfStatement>(
            alsoClause.Range(), LowerExpression(alsoClause.Condition()),
            LowerBlock(alsoClause.Body()), nullptr));
    }
    const source::SourceLocation bodyEnd = statement.Also() == nullptr
                                               ? statement.Body().Range().end
                                               : statement.Also()->Range().end;
    std::unique_ptr<BoundBlockStatement> body =
        std::make_unique<BoundBlockStatement>(
            source::SourceRange{statement.Body().Range().start, bodyEnd},
            std::move(bodyStatements));
    return std::make_unique<BoundWhileStatement>(
        statement.Range(), LowerExpression(statement.Condition()),
        std::move(body));
}

std::unique_ptr<BoundExpression> Lowerer::LowerExpression(
    const core::Expression& expression) {
    const semantic::SemanticType type = _semanticModel->TypeOf(expression);
    switch (expression.Kind()) {
        case core::AstNodeKind::LiteralExpression: {
            const core::LiteralExpression& literal =
                static_cast<const core::LiteralExpression&>(expression);
            return std::make_unique<BoundLiteralExpression>(
                expression.Range(), type, literal.Value());
        }
        case core::AstNodeKind::NameExpression: {
            const core::NameExpression& name =
                static_cast<const core::NameExpression&>(expression);
            const semantic::Symbol* symbol = _semanticModel->SymbolFor(name);
            if (symbol->Kind() == semantic::SymbolKind::Function) {
                return std::make_unique<BoundFunctionExpression>(
                    expression.Range(),
                    static_cast<const semantic::FunctionSymbol&>(*symbol));
            }
            return std::make_unique<BoundVariableExpression>(
                expression.Range(),
                static_cast<const semantic::VariableSymbol&>(*symbol));
        }
        case core::AstNodeKind::ParenthesizedExpression:
            return LowerExpression(
                static_cast<const core::ParenthesizedExpression&>(expression)
                    .Inner());
        case core::AstNodeKind::UnaryExpression: {
            const core::UnaryExpression& unary =
                static_cast<const core::UnaryExpression&>(expression);
            return std::make_unique<BoundUnaryExpression>(
                expression.Range(), type, unary.OperatorType(),
                LowerExpression(unary.Operand()));
        }
        case core::AstNodeKind::BinaryExpression: {
            const core::BinaryExpression& binary =
                static_cast<const core::BinaryExpression&>(expression);
            return std::make_unique<BoundBinaryExpression>(
                expression.Range(), type, LowerExpression(binary.Left()),
                binary.OperatorType(), LowerExpression(binary.Right()));
        }
        case core::AstNodeKind::AssignmentExpression: {
            const core::AssignmentExpression& assignment =
                static_cast<const core::AssignmentExpression&>(expression);
            const core::NameExpression& name =
                static_cast<const core::NameExpression&>(assignment.Target());
            const semantic::VariableSymbol& target =
                static_cast<const semantic::VariableSymbol&>(
                    *_semanticModel->SymbolFor(name));
            return std::make_unique<BoundAssignmentExpression>(
                expression.Range(), target,
                LowerExpression(assignment.Value()));
        }
        case core::AstNodeKind::CallExpression: {
            const core::CallExpression& call =
                static_cast<const core::CallExpression&>(expression);
            const core::NameExpression& callee =
                static_cast<const core::NameExpression&>(call.Callee());
            const semantic::FunctionSymbol& function =
                static_cast<const semantic::FunctionSymbol&>(
                    *_semanticModel->SymbolFor(callee));
            std::vector<std::unique_ptr<BoundExpression>> arguments;
            arguments.reserve(call.Arguments().size());
            for (const std::unique_ptr<core::Expression>& argument :
                 call.Arguments()) {
                arguments.push_back(LowerExpression(*argument));
            }
            return std::make_unique<BoundCallExpression>(
                expression.Range(), function, std::move(arguments));
        }
        default:
            return nullptr;
    }
}

}  // namespace flecha::lowering
