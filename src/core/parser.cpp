#include "core/parser.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace flecha::core {
namespace {

source::SourceRange EndOfInputRange(const std::vector<Token>& tokens) {
    if (tokens.empty()) {
        const source::SourceLocation origin{};
        return {origin, origin};
    }

    const source::SourceLocation end = tokens.back().range.end;
    return {end, end};
}

}  // namespace

Parser::Parser(std::vector<Token> tokens,
               diagnostics::DiagnosticBag& diagnostics)
    : _tokens(std::move(tokens)), _diagnostics(diagnostics) {
    if (_tokens.empty() || _tokens.back().type != TokenType::EOF_TOKEN) {
        _tokens.emplace_back(TokenType::EOF_TOKEN, "",
                             EndOfInputRange(_tokens));
    }
}

const Token& Parser::Current() const { return Peek(0); }

const Token& Parser::Peek(std::size_t offset) const {
    const std::size_t index = std::min(_position + offset, _tokens.size() - 1);
    return _tokens[index];
}

bool Parser::IsAtEnd() const { return Current().type == TokenType::EOF_TOKEN; }

bool Parser::Check(TokenType type) const { return Current().type == type; }

bool Parser::IsInferredVariableDeclaration() const {
    return Check(TokenType::Identifier) &&
           Peek(1).type == TokenType::DeclareAssign;
}

const Token& Parser::Advance() {
    const Token& current = Current();
    if (!IsAtEnd()) {
        ++_position;
    }
    return current;
}

bool Parser::Match(TokenType type) {
    if (!Check(type)) {
        return false;
    }
    Advance();
    return true;
}

bool Parser::IsTypeToken(TokenType type) const {
    switch (type) {
        case TokenType::I8:
        case TokenType::I16:
        case TokenType::I32:
        case TokenType::I64:
        case TokenType::ISize:
        case TokenType::U8:
        case TokenType::U16:
        case TokenType::U32:
        case TokenType::U64:
        case TokenType::USize:
        case TokenType::F32:
        case TokenType::F64:
        case TokenType::Bool:
        case TokenType::Char:
        case TokenType::Void:
            return true;
        default:
            return false;
    }
}

const Token* Parser::Consume(TokenType type, std::string_view expectation) {
    if (Check(type)) {
        return &Advance();
    }

    _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                        Current().range,
                        "expected " + std::string(expectation) + ", found '" +
                            Current().value + "'");
    return nullptr;
}

void Parser::ReportUnexpectedToken(std::string_view context) {
    const std::string tokenText =
        Current().value.empty() ? "end of file" : "'" + Current().value + "'";
    _diagnostics.Report(diagnostics::DiagnosticCode::UnexpectedToken,
                        Current().range,
                        "unexpected " + tokenText + " " + std::string(context));
}

std::unique_ptr<TypeNode> Parser::ParseType() {
    const Token& token = Current();
    PrimitiveType type;

    switch (token.type) {
        case TokenType::I8:
            type = PrimitiveType::I8;
            break;
        case TokenType::I16:
            type = PrimitiveType::I16;
            break;
        case TokenType::I32:
            type = PrimitiveType::I32;
            break;
        case TokenType::I64:
            type = PrimitiveType::I64;
            break;
        case TokenType::ISize:
            type = PrimitiveType::ISize;
            break;
        case TokenType::U8:
            type = PrimitiveType::U8;
            break;
        case TokenType::U16:
            type = PrimitiveType::U16;
            break;
        case TokenType::U32:
            type = PrimitiveType::U32;
            break;
        case TokenType::U64:
            type = PrimitiveType::U64;
            break;
        case TokenType::USize:
            type = PrimitiveType::USize;
            break;
        case TokenType::F32:
            type = PrimitiveType::F32;
            break;
        case TokenType::F64:
            type = PrimitiveType::F64;
            break;
        case TokenType::Bool:
            type = PrimitiveType::Bool;
            break;
        case TokenType::Char:
            type = PrimitiveType::Char;
            break;
        case TokenType::Void:
            type = PrimitiveType::Void;
            break;
        default:
            _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                token.range, "expected a type");
            return nullptr;
    }

    Advance();
    return std::make_unique<TypeNode>(token.range, type);
}

std::unique_ptr<Parameter> Parser::ParseParameter() {
    const source::SourceLocation start = Current().range.start;
    std::unique_ptr<TypeNode> type = ParseType();
    if (type == nullptr) {
        return nullptr;
    }

    const Token* name = Consume(TokenType::Identifier, "parameter name");
    if (name == nullptr) {
        return nullptr;
    }

    return std::make_unique<Parameter>(
        source::SourceRange{start, name->range.end}, std::move(type),
        name->value, name->range);
}

std::unique_ptr<FunctionDeclaration> Parser::ParseFunctionDeclaration() {
    const source::SourceLocation start = Current().range.start;
    std::unique_ptr<TypeNode> returnType = ParseType();
    if (returnType == nullptr) {
        return nullptr;
    }

    const Token* name = Consume(TokenType::Identifier, "function name");
    if (name == nullptr) {
        return nullptr;
    }
    if (Consume(TokenType::LParen, "'(' after function name") == nullptr) {
        return nullptr;
    }

    std::vector<std::unique_ptr<Parameter>> parameters;
    while (!Check(TokenType::RParen) && !IsAtEnd()) {
        std::unique_ptr<Parameter> parameter = ParseParameter();
        if (parameter != nullptr) {
            parameters.push_back(std::move(parameter));
        } else {
            while (!IsAtEnd() && !Check(TokenType::Comma) &&
                   !Check(TokenType::RParen)) {
                Advance();
            }
        }

        if (!Match(TokenType::Comma)) {
            break;
        }
    }

    if (Consume(TokenType::RParen, "')' after parameters") == nullptr) {
        return nullptr;
    }

    std::unique_ptr<BlockStatement> body = ParseBlockStatement();
    if (body == nullptr) {
        return nullptr;
    }
    const source::SourceLocation end = body->Range().end;

    return std::make_unique<FunctionDeclaration>(
        source::SourceRange{start, end}, std::vector<FunctionModifier>{},
        std::move(returnType), name->value, std::move(parameters),
        std::move(body), name->range);
}

std::unique_ptr<Declaration> Parser::ParseDeclaration() {
    if (IsTypeToken(Current().type)) {
        return ParseFunctionDeclaration();
    }

    ReportUnexpectedToken("at top level");
    return nullptr;
}

std::unique_ptr<BlockStatement> Parser::ParseBlockStatement() {
    const Token* openBrace = Consume(TokenType::LCurly, "'{' to start block");
    if (openBrace == nullptr) {
        return nullptr;
    }

    std::vector<std::unique_ptr<Statement>> statements;
    while (!Check(TokenType::RCurly) && !IsAtEnd()) {
        std::unique_ptr<Statement> statement = ParseStatement();
        if (statement != nullptr) {
            statements.push_back(std::move(statement));
        } else {
            SynchronizeStatement();
        }
    }

    const Token* closeBrace = Consume(TokenType::RCurly, "'}' after block");
    const source::SourceLocation end =
        closeBrace == nullptr ? Current().range.end : closeBrace->range.end;
    return std::make_unique<BlockStatement>(
        source::SourceRange{openBrace->range.start, end},
        std::move(statements));
}

std::unique_ptr<Statement> Parser::ParseStatement() {
    if (Check(TokenType::Return)) {
        return ParseReturnStatement();
    }
    if (Check(TokenType::If)) {
        return ParseIfStatement();
    }
    if (Check(TokenType::While)) {
        return ParseWhileStatement();
    }
    if (IsTypeToken(Current().type) || IsInferredVariableDeclaration()) {
        return ParseVariableDeclarationStatement();
    }

    const source::SourceLocation start = Current().range.start;
    std::unique_ptr<Expression> expression = ParseExpression();
    if (expression == nullptr) {
        return nullptr;
    }
    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after expression");
    const source::SourceLocation end =
        semicolon == nullptr ? expression->Range().end : semicolon->range.end;
    return std::make_unique<ExpressionStatement>(
        source::SourceRange{start, end}, std::move(expression));
}

std::unique_ptr<VariableDeclarationStatement>
Parser::ParseVariableDeclarationStatement() {
    const source::SourceLocation start = Current().range.start;
    std::unique_ptr<TypeNode> type;
    const Token* name = nullptr;
    if (IsInferredVariableDeclaration()) {
        name = &Advance();
        Advance();
    } else {
        type = ParseType();
        if (type == nullptr) {
            return nullptr;
        }

        name = Consume(TokenType::Identifier, "variable name");
        if (name == nullptr) {
            return nullptr;
        }
        if (Consume(TokenType::Assign, "'=' after variable name") == nullptr) {
            return nullptr;
        }
    }

    std::unique_ptr<Expression> initializer = ParseExpression();
    if (initializer == nullptr) {
        return nullptr;
    }

    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after variable declaration");
    const source::SourceLocation end =
        semicolon == nullptr ? initializer->Range().end : semicolon->range.end;
    return std::make_unique<VariableDeclarationStatement>(
        source::SourceRange{start, end}, std::move(type), name->value,
        std::move(initializer), name->range);
}

std::unique_ptr<AlsoClause> Parser::ParseAlsoClause() {
    const Token& alsoToken = Advance();
    if (Check(TokenType::LCurly)) {
        _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                            Current().range,
                            "expected a condition after 'also'");
        ParseBlockStatement();
        return nullptr;
    }

    std::unique_ptr<Expression> condition = ParseExpression();
    if (condition == nullptr) {
        return nullptr;
    }
    std::unique_ptr<BlockStatement> body = ParseBlockStatement();
    if (body == nullptr) {
        return nullptr;
    }
    const source::SourceLocation end = body->Range().end;
    return std::make_unique<AlsoClause>(
        source::SourceRange{alsoToken.range.start, end}, std::move(condition),
        std::move(body));
}

std::unique_ptr<ConditionalBranch> Parser::ParseConditionalBranch(
    source::SourceLocation start) {
    std::unique_ptr<Expression> condition = ParseExpression();
    if (condition == nullptr) {
        return nullptr;
    }
    std::unique_ptr<BlockStatement> body = ParseBlockStatement();
    if (body == nullptr) {
        return nullptr;
    }

    source::SourceLocation end = body->Range().end;
    std::unique_ptr<AlsoClause> alsoClause;
    if (Check(TokenType::Also)) {
        alsoClause = ParseAlsoClause();
        if (alsoClause != nullptr) {
            end = alsoClause->Range().end;
        }
    }

    while (Check(TokenType::Also)) {
        _diagnostics.Report(diagnostics::DiagnosticCode::UnexpectedToken,
                            Current().range,
                            "a branch may have only one 'also' clause");
        std::unique_ptr<AlsoClause> extraClause = ParseAlsoClause();
        if (extraClause != nullptr) {
            end = extraClause->Range().end;
        }
    }

    return std::make_unique<ConditionalBranch>(
        source::SourceRange{start, end}, std::move(condition), std::move(body),
        std::move(alsoClause));
}

std::unique_ptr<IfStatement> Parser::ParseIfStatement() {
    const Token& ifToken = Advance();
    std::unique_ptr<ConditionalBranch> primaryBranch =
        ParseConditionalBranch(ifToken.range.start);
    if (primaryBranch == nullptr) {
        return nullptr;
    }

    source::SourceLocation end = primaryBranch->Range().end;
    std::vector<std::unique_ptr<ConditionalBranch>> elseIfBranches;
    std::unique_ptr<BlockStatement> elseBody;

    while (Check(TokenType::Else)) {
        const Token& elseToken = Advance();
        if (Match(TokenType::If)) {
            std::unique_ptr<ConditionalBranch> branch =
                ParseConditionalBranch(elseToken.range.start);
            if (branch == nullptr) {
                return nullptr;
            }
            end = branch->Range().end;
            elseIfBranches.push_back(std::move(branch));
            continue;
        }

        elseBody = ParseBlockStatement();
        if (elseBody != nullptr) {
            end = elseBody->Range().end;
        }
        break;
    }

    return std::make_unique<IfStatement>(
        source::SourceRange{ifToken.range.start, end}, std::move(primaryBranch),
        std::move(elseIfBranches), std::move(elseBody));
}

std::unique_ptr<WhileStatement> Parser::ParseWhileStatement() {
    const Token& whileToken = Advance();
    std::unique_ptr<Expression> condition = ParseExpression();
    if (condition == nullptr) return nullptr;

    std::unique_ptr<BlockStatement> body = ParseBlockStatement();
    if (body == nullptr) return nullptr;

    source::SourceLocation end = body->Range().end;
    std::unique_ptr<AlsoClause> alsoClause;
    if (Check(TokenType::Also)) {
        alsoClause = ParseAlsoClause();
        if (alsoClause != nullptr) end = alsoClause->Range().end;
    }

    while (Check(TokenType::Also)) {
        _diagnostics.Report(diagnostics::DiagnosticCode::UnexpectedToken,
                            Current().range,
                            "a while loop may have only one 'also' clause");
        std::unique_ptr<AlsoClause> extraClause = ParseAlsoClause();
        if (extraClause != nullptr) end = extraClause->Range().end;
    }

    return std::make_unique<WhileStatement>(
        source::SourceRange{whileToken.range.start, end}, std::move(condition),
        std::move(body), std::move(alsoClause));
}

std::unique_ptr<ReturnStatement> Parser::ParseReturnStatement() {
    const Token& returnToken = Advance();
    std::unique_ptr<Expression> expression;
    if (!Check(TokenType::SemiColon)) {
        expression = ParseExpression();
    }

    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after return statement");
    const source::SourceLocation end =
        semicolon == nullptr ? Current().range.start : semicolon->range.end;
    return std::make_unique<ReturnStatement>(
        source::SourceRange{returnToken.range.start, end},
        std::move(expression));
}

std::unique_ptr<Expression> Parser::ParseExpression() {
    return ParseAssignmentExpression();
}

std::unique_ptr<Expression> Parser::ParseAssignmentExpression() {
    std::unique_ptr<Expression> target = ParseBinaryExpression(1);
    if (target == nullptr || !Match(TokenType::Assign)) {
        return target;
    }

    const source::SourceRange targetRange = target->Range();
    if (target->Kind() != AstNodeKind::NameExpression) {
        _diagnostics.Report(
            diagnostics::DiagnosticCode::InvalidAssignmentTarget, targetRange,
            "expression is not a valid assignment target");
    }

    std::unique_ptr<Expression> value = ParseAssignmentExpression();
    if (value == nullptr) {
        return target;
    }

    return std::make_unique<AssignmentExpression>(
        source::SourceRange{targetRange.start, value->Range().end},
        std::move(target), std::move(value));
}

int Parser::BinaryPrecedence(TokenType type) const {
    switch (type) {
        case TokenType::Or:
            return 1;
        case TokenType::And:
            return 2;
        case TokenType::Xor:
            return 3;
        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::StrictEqual:
        case TokenType::StrictNotEqual:
            return 4;
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
            return 5;
        case TokenType::Add:
        case TokenType::Sub:
            return 6;
        case TokenType::Mul:
        case TokenType::Div:
        case TokenType::Mod:
            return 7;
        default:
            return 0;
    }
}

int Parser::UnaryPrecedence(TokenType type) const {
    switch (type) {
        case TokenType::Add:
        case TokenType::Sub:
        case TokenType::Bang:
        case TokenType::Mul:
            return 8;
        default:
            return 0;
    }
}

std::unique_ptr<Expression> Parser::ParseBinaryExpression(
    int minimumPrecedence) {
    std::unique_ptr<Expression> left = ParseUnaryExpression();
    if (left == nullptr) {
        return nullptr;
    }

    while (true) {
        const int precedence = BinaryPrecedence(Current().type);
        if (precedence < minimumPrecedence) {
            break;
        }

        const Token& operatorToken = Advance();
        std::unique_ptr<Expression> right =
            ParseBinaryExpression(precedence + 1);
        if (right == nullptr) {
            return left;
        }

        const source::SourceRange range{left->Range().start,
                                        right->Range().end};
        left = std::make_unique<BinaryExpression>(
            range, std::move(left), operatorToken.type, std::move(right));
    }

    return left;
}

std::unique_ptr<Expression> Parser::ParseUnaryExpression() {
    if (UnaryPrecedence(Current().type) == 0) {
        return ParsePostfixExpression();
    }

    const Token& operatorToken = Advance();
    std::unique_ptr<Expression> operand = ParseUnaryExpression();
    if (operand == nullptr) {
        return nullptr;
    }

    return std::make_unique<UnaryExpression>(
        source::SourceRange{operatorToken.range.start, operand->Range().end},
        operatorToken.type, std::move(operand));
}

std::unique_ptr<Expression> Parser::ParsePostfixExpression() {
    std::unique_ptr<Expression> expression = ParsePrimaryExpression();
    if (expression == nullptr) {
        return nullptr;
    }

    while (Check(TokenType::LParen)) {
        Advance();
        std::vector<std::unique_ptr<Expression>> arguments;

        while (!Check(TokenType::RParen) && !IsAtEnd()) {
            std::unique_ptr<Expression> argument = ParseExpression();
            if (argument == nullptr) {
                while (!IsAtEnd() && !Check(TokenType::Comma) &&
                       !Check(TokenType::RParen) &&
                       !Check(TokenType::SemiColon)) {
                    Advance();
                }
            } else {
                arguments.push_back(std::move(argument));
            }

            if (Match(TokenType::Comma)) {
                if (Check(TokenType::RParen)) {
                    _diagnostics.Report(
                        diagnostics::DiagnosticCode::ExpectedToken,
                        Current().range, "expected an argument after ','");
                }
                continue;
            }
            if (!Check(TokenType::RParen)) {
                _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                    Current().range,
                                    "expected ',' or ')' after argument");
                while (!IsAtEnd() && !Check(TokenType::RParen) &&
                       !Check(TokenType::SemiColon)) {
                    Advance();
                }
            }
            break;
        }

        const Token* closeParen =
            Consume(TokenType::RParen, "')' after arguments");
        source::SourceLocation end = expression->Range().end;
        if (closeParen != nullptr) {
            end = closeParen->range.end;
        } else if (!arguments.empty()) {
            end = arguments.back()->Range().end;
        }

        expression = std::make_unique<CallExpression>(
            source::SourceRange{expression->Range().start, end},
            std::move(expression), std::move(arguments));
    }

    return expression;
}

std::unique_ptr<Expression> Parser::ParsePrimaryExpression() {
    const Token& token = Current();
    switch (token.type) {
        case TokenType::NumberLiteral:
        case TokenType::FloatLiteral:
        case TokenType::StringLiteral:
        case TokenType::CharLiteral:
        case TokenType::True:
        case TokenType::False:
        case TokenType::None:
        case TokenType::Null:
            Advance();
            return std::make_unique<LiteralExpression>(token.range, token.type,
                                                       token.value);
        case TokenType::Identifier:
            Advance();
            return std::make_unique<NameExpression>(token.range, token.value);
        case TokenType::LParen: {
            const Token& openParen = Advance();
            std::unique_ptr<Expression> expression = ParseExpression();
            if (expression == nullptr) {
                return nullptr;
            }
            const Token* closeParen =
                Consume(TokenType::RParen, "')' after expression");
            const source::SourceLocation end = closeParen == nullptr
                                                   ? expression->Range().end
                                                   : closeParen->range.end;
            return std::make_unique<ParenthesizedExpression>(
                source::SourceRange{openParen.range.start, end},
                std::move(expression));
        }
        default:
            _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                token.range, "expected an expression");
            return nullptr;
    }
}

void Parser::SynchronizeTopLevel() {
    if (!IsAtEnd()) Advance();
    while (!IsAtEnd() && !IsTypeToken(Current().type)) {
        Advance();
    }
}

void Parser::SynchronizeStatement() {
    if (!IsAtEnd()) Advance();
    while (!IsAtEnd() && !Check(TokenType::SemiColon) &&
           !Check(TokenType::RCurly)) {
        Advance();
    }
    Match(TokenType::SemiColon);
}

std::unique_ptr<CompilationUnit> Parser::Parse() {
    const source::SourceLocation start = Current().range.start;
    std::vector<std::unique_ptr<Declaration>> declarations;

    while (!IsAtEnd()) {
        const std::size_t startPosition = _position;
        std::unique_ptr<Declaration> declaration = ParseDeclaration();
        if (declaration != nullptr) {
            declarations.push_back(std::move(declaration));
        } else {
            SynchronizeTopLevel();
        }
        if (_position == startPosition && !IsAtEnd()) {
            Advance();
        }
    }

    return std::make_unique<CompilationUnit>(
        source::SourceRange{start, Current().range.end},
        std::move(declarations));
}

}  // namespace flecha::core
