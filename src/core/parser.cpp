#include "core/parser.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

namespace flecha::core {
namespace {

std::unique_ptr<TypeNode> CloneType(const TypeNode& type) {
    if (type.TypeForm() == TypeKind::Primitive)
        return std::make_unique<TypeNode>(
            type.Range(), type.Type(), type.IsConst(), type.QualifierRange());
    return std::make_unique<TypeNode>(type.Range(), type.TypeForm(),
                                      CloneType(*type.Target()), type.IsConst(),
                                      type.QualifierRange(), type.Length());
}

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
        case TokenType::String:
        case TokenType::StringView:
            return true;
        default:
            return false;
    }
}

bool Parser::StartsType() const {
    if (Check(TokenType::Const) || Check(TokenType::ConstPtr) ||
        Check(TokenType::ConstRef))
        return true;
    std::size_t offset = 0;
    for (;;) {
        const TokenType token = Peek(offset).type;
        if (token == TokenType::Mul || token == TokenType::Ampersand ||
            token == TokenType::Const || token == TokenType::ConstPtr ||
            token == TokenType::ConstRef) {
            ++offset;
        } else if (token == TokenType::LBracket) {
            // Look through the bracketed count to distinguish [N]T from an
            // array literal expression, including malformed count syntax.
            ++offset;
            while (Peek(offset).type != TokenType::RBracket) {
                const TokenType next = Peek(offset).type;
                if (next == TokenType::EOF_TOKEN ||
                    next == TokenType::SemiColon || next == TokenType::LCurly ||
                    next == TokenType::RCurly)
                    return false;
                ++offset;
            }
            ++offset;
        } else
            break;
    }
    return IsTypeToken(Peek(offset).type);
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
    const source::SourceLocation start = Current().range.start;
    source::SourceRange qualifierRange;
    const bool explicitConst = Check(TokenType::Const);
    if (explicitConst) qualifierRange = Advance().range;
    const Token& token = Current();
    const bool alias =
        token.type == TokenType::ConstPtr || token.type == TokenType::ConstRef;
    if (explicitConst && alias) {
        _diagnostics.Report(diagnostics::DiagnosticCode::UnexpectedToken,
                            token.range, "duplicate const qualifier");
        return nullptr;
    }
    if (token.type == TokenType::Mul || token.type == TokenType::Ampersand ||
        alias) {
        Advance();
        std::unique_ptr<TypeNode> target = ParseType();
        if (target == nullptr) return nullptr;
        const source::SourceRange range{start, target->Range().end};
        const TypeKind kind =
            token.type == TokenType::Mul || token.type == TokenType::ConstPtr
                ? TypeKind::Pointer
                : TypeKind::Reference;
        return std::make_unique<TypeNode>(range, kind, std::move(target),
                                          explicitConst || alias,
                                          alias ? token.range : qualifierRange);
    }
    if (Match(TokenType::LBracket)) {
        if (Match(TokenType::RBracket)) {
            auto target = ParseType();
            if (target == nullptr) return nullptr;
            const source::SourceRange range{start, target->Range().end};
            return std::make_unique<TypeNode>(range, TypeKind::Slice,
                                              std::move(target), explicitConst,
                                              qualifierRange);
        }
        const std::optional<std::uint64_t> length = ParseArrayLength();
        if (!length.has_value() ||
            Consume(TokenType::RBracket, "']' after array length") == nullptr)
            return nullptr;
        std::unique_ptr<TypeNode> target = ParseType();
        if (target == nullptr) return nullptr;
        const source::SourceRange range{start, target->Range().end};
        return std::make_unique<TypeNode>(range, TypeKind::Array,
                                          std::move(target), explicitConst,
                                          qualifierRange, *length);
    }
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
        case TokenType::String:
            type = PrimitiveType::String;
            break;
        case TokenType::StringView:
            type = PrimitiveType::StringView;
            break;
        default:
            _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                token.range, "expected a type");
            return nullptr;
    }

    Advance();
    return std::make_unique<TypeNode>(
        source::SourceRange{start, token.range.end}, type, explicitConst,
        qualifierRange);
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
    std::vector<FunctionModifier> modifiers;
    if (Match(TokenType::Unsafe)) modifiers.push_back(FunctionModifier::Unsafe);
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
        source::SourceRange{start, end}, std::move(modifiers),
        std::move(returnType), name->value, std::move(parameters),
        std::move(body), name->range);
}

std::unique_ptr<Declaration> Parser::ParseDeclaration() {
    if (StartsType() || Check(TokenType::Unsafe)) {
        return ParseFunctionDeclaration();
    }

    ReportUnexpectedToken("at top level");
    return nullptr;
}

std::unique_ptr<BlockStatement> Parser::ParseBlockStatement(bool isUnsafe) {
    const source::SourceLocation start = Current().range.start;
    if (isUnsafe) Advance();
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
    return std::make_unique<BlockStatement>(source::SourceRange{start, end},
                                            std::move(statements), isUnsafe);
}

std::unique_ptr<Statement> Parser::ParseStatement() {
    if (Check(TokenType::LCurly)) return ParseBlockStatement();
    if (Check(TokenType::Unsafe)) return ParseBlockStatement(true);
    if (Check(TokenType::Return)) {
        return ParseReturnStatement();
    }
    if (Check(TokenType::If)) {
        return ParseIfStatement();
    }
    if (Check(TokenType::While)) {
        return ParseWhileStatement();
    }
    if (Check(TokenType::Loop)) return ParseLoopStatement();
    if (Check(TokenType::For)) return ParseRangeStatement();
    if (Check(TokenType::Proceed)) return ParseProceedStatement();
    if (Check(TokenType::Break)) return ParseBreakStatement();
    if (Check(TokenType::JumpTo)) return ParseJumpToStatement();
    if (Check(TokenType::Hash)) return ParsePositionStatement();
    if (StartsType() || IsInferredVariableDeclaration()) {
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
    bool hasInitializer = false;
    if (IsInferredVariableDeclaration()) {
        name = &Advance();
        Advance();
        hasInitializer = true;
    } else {
        type = ParseType();
        if (type == nullptr) {
            return nullptr;
        }

        name = Consume(TokenType::Identifier, "variable name");
        if (name == nullptr) {
            return nullptr;
        }
        if (Check(TokenType::Assign)) {
            Advance();
            hasInitializer = true;
        }
    }

    std::unique_ptr<Expression> initializer;
    if (hasInitializer) {
        initializer = ParseExpression();
        if (initializer == nullptr) return nullptr;
    }

    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after variable declaration");
    const source::SourceLocation end =
        semicolon == nullptr
            ? (initializer == nullptr ? name->range.end
                                      : initializer->Range().end)
            : semicolon->range.end;
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

std::unique_ptr<Statement> Parser::ParseLoopStatement() {
    const Token& keyword = Advance();
    std::vector<std::unique_ptr<Statement>> statements;
    if (!Match(TokenType::SemiColon)) {
        if (StartsType() || IsInferredVariableDeclaration()) {
            auto initializer = ParseVariableDeclarationStatement();
            if (initializer == nullptr) return nullptr;
            statements.push_back(std::move(initializer));
        } else {
            auto initializer = ParseExpression();
            if (initializer == nullptr) return nullptr;
            const source::SourceRange range = initializer->Range();
            statements.push_back(std::make_unique<ExpressionStatement>(
                range, std::move(initializer)));
            if (Consume(TokenType::SemiColon, "';' after loop initializer") ==
                nullptr)
                return nullptr;
        }
    }
    std::unique_ptr<Expression> condition;
    if (Check(TokenType::SemiColon))
        condition = std::make_unique<LiteralExpression>(
            Current().range, TokenType::True, "true");
    else
        condition = ParseExpression();
    if (condition == nullptr ||
        Consume(TokenType::SemiColon, "';' after loop condition") == nullptr)
        return nullptr;
    std::unique_ptr<Expression> update;
    if (!Check(TokenType::LCurly)) {
        update = ParseExpression();
        if (update == nullptr) return nullptr;
    }
    auto body = ParseBlockStatement();
    if (body == nullptr) return nullptr;
    const source::SourceRange range{keyword.range.start, body->Range().end};
    statements.push_back(std::make_unique<WhileStatement>(
        range, std::move(condition), std::move(body), nullptr,
        std::move(update)));
    return std::make_unique<BlockStatement>(range, std::move(statements));
}

std::unique_ptr<RangeStatement> Parser::ParseRangeStatement() {
    const Token& keyword = Advance();
    const bool pair = Match(TokenType::LParen);
    const bool typed = StartsType();
    std::vector<std::unique_ptr<TypeNode>> types;
    std::vector<const Token*> names;
    for (std::size_t binding = 0; binding < (pair ? 2u : 1u); ++binding) {
        types.push_back(typed ? ParseType() : nullptr);
        if (typed && types.back() == nullptr) return nullptr;
        const Token* name =
            Consume(TokenType::Identifier, "range binding name");
        if (name == nullptr) return nullptr;
        names.push_back(name);
        if (pair && binding == 0 &&
            Consume(TokenType::Comma, "',' between range bindings") == nullptr)
            return nullptr;
    }
    if (pair &&
        Consume(TokenType::RParen, "')' after range bindings") == nullptr)
        return nullptr;
    if (Consume(typed ? TokenType::Assign : TokenType::DeclareAssign,
                typed ? "'=' after typed range bindings"
                      : "':=' after inferred range bindings") == nullptr)
        return nullptr;
    if (Consume(TokenType::Range, "'range' before array expression") == nullptr)
        return nullptr;
    auto source = ParseExpression();
    if (source == nullptr) return nullptr;
    const source::SourceRange sourceRange = source->Range();
    auto body = ParseBlockStatement();
    if (body == nullptr) return nullptr;
    const source::SourceRange range{keyword.range.start, body->Range().end};
    // '$' cannot occur in source identifiers, so these names cannot collide.
    const std::string suffix = std::to_string(_rangeId++);
    const std::string sourceName = "$range" + suffix;
    const std::string cursorName = "$cursor" + suffix;
    const auto Name = [&](const std::string& name) {
        return std::make_unique<NameExpression>(sourceRange, name);
    };
    std::vector<std::unique_ptr<Statement>> outer;
    auto snapshot = std::make_unique<VariableDeclarationStatement>(
        sourceRange, nullptr, sourceName, std::move(source), sourceRange, true);
    const VariableDeclarationStatement* sourceDeclaration = snapshot.get();
    outer.push_back(std::move(snapshot));
    outer.push_back(std::make_unique<VariableDeclarationStatement>(
        range, std::make_unique<TypeNode>(range, PrimitiveType::USize),
        cursorName,
        std::make_unique<LiteralExpression>(range, TokenType::NumberLiteral,
                                            "0"),
        range));
    std::vector<std::unique_ptr<Statement>> iteration;
    const VariableDeclarationStatement* indexBinding = nullptr;
    for (std::size_t binding = 0; binding < names.size(); ++binding) {
        std::unique_ptr<Expression> value;
        if (binding == 0) {
            value = Name(cursorName);
            // A typed index uses a checked-at-analysis conversion. The cursor
            // itself remains usize even when the exposed index is narrower.
            if (types[binding] != nullptr) {
                value = std::make_unique<CastExpression>(
                    names[binding]->range, std::move(value),
                    CloneType(*types[binding]));
            }
        } else {
            value = std::make_unique<IndexExpression>(
                sourceRange, Name(sourceName), Name(cursorName), true);
        }
        auto declaration = std::make_unique<VariableDeclarationStatement>(
            names[binding]->range, std::move(types[binding]),
            names[binding]->value == "_"
                ? "$discard" + suffix + "." + std::to_string(binding)
                : names[binding]->value,
            std::move(value), names[binding]->range);
        if (binding == 0) indexBinding = declaration.get();
        iteration.push_back(std::move(declaration));
    }
    iteration.push_back(std::move(body));
    auto condition = std::make_unique<BinaryExpression>(
        range, Name(cursorName), TokenType::Less,
        std::make_unique<MemberCallExpression>(
            sourceRange, Name(sourceName), "len", sourceRange,
            std::vector<std::unique_ptr<Expression>>{}));
    auto update = std::make_unique<UnaryExpression>(range, TokenType::Increment,
                                                    Name(cursorName));
    outer.push_back(std::make_unique<WhileStatement>(
        range, std::move(condition),
        std::make_unique<BlockStatement>(range, std::move(iteration)), nullptr,
        std::move(update)));
    return std::make_unique<RangeStatement>(
        range, std::make_unique<BlockStatement>(range, std::move(outer)),
        sourceDeclaration, indexBinding);
}

std::unique_ptr<ProceedStatement> Parser::ParseProceedStatement() {
    const Token& keyword = Advance();
    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after 'proceed'");
    const source::SourceLocation end =
        semicolon == nullptr ? keyword.range.end : semicolon->range.end;
    return std::make_unique<ProceedStatement>(
        source::SourceRange{keyword.range.start, end});
}

std::unique_ptr<BreakStatement> Parser::ParseBreakStatement() {
    const Token& keyword = Advance();
    const Token* semicolon = Consume(TokenType::SemiColon, "';' after 'break'");
    const source::SourceLocation end =
        semicolon == nullptr ? keyword.range.end : semicolon->range.end;
    return std::make_unique<BreakStatement>(
        source::SourceRange{keyword.range.start, end});
}

std::unique_ptr<JumpToStatement> Parser::ParseJumpToStatement() {
    const Token& keyword = Advance();
    const Token* position = Consume(TokenType::Identifier, "position name");
    if (position == nullptr) return nullptr;
    const Token* semicolon =
        Consume(TokenType::SemiColon, "';' after position name");
    const source::SourceLocation end =
        semicolon == nullptr ? position->range.end : semicolon->range.end;
    return std::make_unique<JumpToStatement>(
        source::SourceRange{keyword.range.start, end}, position->value,
        position->range);
}

std::unique_ptr<PositionStatement> Parser::ParsePositionStatement() {
    const Token& hash = Advance();
    const Token* name =
        Consume(TokenType::Identifier, "position name after '#'");
    if (name == nullptr) return nullptr;
    return std::make_unique<PositionStatement>(
        source::SourceRange{hash.range.start, name->range.end}, name->value,
        name->range);
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
    const Expression* place = target.get();
    while (place->Kind() == AstNodeKind::ParenthesizedExpression) {
        place = &static_cast<const ParenthesizedExpression&>(*place).Inner();
    }
    const bool dereference =
        place->Kind() == AstNodeKind::UnaryExpression &&
        static_cast<const UnaryExpression&>(*place).OperatorType() ==
            TokenType::Mul;
    if (place->Kind() != AstNodeKind::NameExpression &&
        place->Kind() != AstNodeKind::IndexExpression &&
        place->Kind() != AstNodeKind::MemberCallExpression && !dereference) {
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
        case TokenType::Ampersand:
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
    TokenType operation = operatorToken.type;
    if (operation == TokenType::Ampersand && Check(TokenType::Identifier) &&
        Current().value == "raw") {
        Advance();
        operation = TokenType::RawAddress;
    }
    std::unique_ptr<Expression> operand = ParseUnaryExpression();
    if (operand == nullptr) {
        return nullptr;
    }

    return std::make_unique<UnaryExpression>(
        source::SourceRange{operatorToken.range.start, operand->Range().end},
        operation, std::move(operand));
}

std::unique_ptr<Expression> Parser::ParsePostfixExpression() {
    std::unique_ptr<Expression> expression = ParsePrimaryExpression();
    if (expression == nullptr) {
        return nullptr;
    }

    while (true) {
        if (Check(TokenType::Increment)) {
            const Token& operation = Advance();
            expression = std::make_unique<UnaryExpression>(
                source::SourceRange{expression->Range().start,
                                    operation.range.end},
                TokenType::Increment, std::move(expression));
            continue;
        }
        if (Match(TokenType::As)) {
            std::unique_ptr<TypeNode> targetType = ParseType();
            if (targetType == nullptr) return expression;
            const source::SourceLocation end = targetType->Range().end;
            expression = std::make_unique<CastExpression>(
                source::SourceRange{expression->Range().start, end},
                std::move(expression), std::move(targetType));
            continue;
        }

        if (Match(TokenType::Dot)) {
            const Token* member = nullptr;
            if (Check(TokenType::Identifier) || Check(TokenType::String)) {
                member = &Advance();
            } else {
                member =
                    Consume(TokenType::Identifier, "member name after '.'");
            }
            if (member == nullptr) return expression;
            if (member->value == "cast" && Match(TokenType::Less)) {
                std::unique_ptr<TypeNode> targetType = ParseType();
                if (targetType == nullptr) return expression;
                const Token* close =
                    Consume(TokenType::Greater, "'>' after cast type");
                const source::SourceLocation end = close == nullptr
                                                       ? targetType->Range().end
                                                       : close->range.end;
                expression = std::make_unique<CastExpression>(
                    source::SourceRange{expression->Range().start, end},
                    std::move(expression), std::move(targetType));
                continue;
            }
            if (!Match(TokenType::LParen)) {
                _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                    Current().range,
                                    "expected '(' after member name");
                return expression;
            }
            source::SourceLocation end = member->range.end;
            std::vector<std::unique_ptr<Expression>> arguments =
                ParseArgumentList(end);
            expression = std::make_unique<MemberCallExpression>(
                source::SourceRange{expression->Range().start, end},
                std::move(expression), member->value, member->range,
                std::move(arguments));
            continue;
        }

        if (Match(TokenType::LBracket)) {
            std::unique_ptr<Expression> index;
            if (!Check(TokenType::Colon)) {
                index = ParseExpression();
                if (index == nullptr) return expression;
            }
            if (Match(TokenType::Colon)) {
                std::unique_ptr<Expression> upper;
                if (!Check(TokenType::RBracket)) {
                    upper = ParseExpression();
                    if (upper == nullptr) return expression;
                }
                const Token* close =
                    Consume(TokenType::RBracket, "']' after slice bounds");
                const source::SourceLocation end =
                    close == nullptr ? Current().range.start : close->range.end;
                expression = std::make_unique<SliceExpression>(
                    source::SourceRange{expression->Range().start, end},
                    std::move(expression), std::move(index), std::move(upper));
                continue;
            }
            const Token* close =
                Consume(TokenType::RBracket, "']' after index");
            const source::SourceLocation end =
                close == nullptr ? index->Range().end : close->range.end;
            expression = std::make_unique<IndexExpression>(
                source::SourceRange{expression->Range().start, end},
                std::move(expression), std::move(index));
            continue;
        }

        if (!Match(TokenType::LParen)) break;
        source::SourceLocation end = expression->Range().end;
        std::vector<std::unique_ptr<Expression>> arguments =
            ParseArgumentList(end);
        expression = std::make_unique<CallExpression>(
            source::SourceRange{expression->Range().start, end},
            std::move(expression), std::move(arguments));
    }

    return expression;
}

std::vector<std::unique_ptr<Expression>> Parser::ParseArgumentList(
    source::SourceLocation& end) {
    std::vector<std::unique_ptr<Expression>> arguments;
    while (!Check(TokenType::RParen) && !IsAtEnd()) {
        std::unique_ptr<Expression> argument = ParseExpression();
        if (argument == nullptr) {
            while (!IsAtEnd() && !Check(TokenType::Comma) &&
                   !Check(TokenType::RParen) && !Check(TokenType::SemiColon)) {
                Advance();
            }
        } else {
            end = argument->Range().end;
            arguments.push_back(std::move(argument));
        }

        if (Match(TokenType::Comma)) {
            if (Check(TokenType::RParen)) {
                _diagnostics.Report(diagnostics::DiagnosticCode::ExpectedToken,
                                    Current().range,
                                    "expected an argument after ','");
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

    const Token* closeParen = Consume(TokenType::RParen, "')' after arguments");
    if (closeParen != nullptr) end = closeParen->range.end;
    return arguments;
}

std::optional<std::uint64_t> Parser::ParseArrayLength() {
    const Token* count = Consume(TokenType::NumberLiteral,
                                 "nonnegative integer literal array length");
    if (count == nullptr) return std::nullopt;
    std::uint64_t length = 0;
    const auto result = std::from_chars(
        count->value.data(), count->value.data() + count->value.size(), length);
    if (result.ec != std::errc{} ||
        result.ptr != count->value.data() + count->value.size()) {
        _diagnostics.Report(diagnostics::DiagnosticCode::InvalidArrayLength,
                            count->range, "array length is too large");
        return std::nullopt;
    }
    return length;
}

std::unique_ptr<Expression> Parser::ParseArrayExpression() {
    const source::SourceLocation start = Advance().range.start;
    std::vector<std::unique_ptr<Expression>> elements;
    bool repeat = false;
    std::uint64_t length = 0;
    if (!Check(TokenType::RBracket)) {
        for (;;) {
            std::unique_ptr<Expression> element = ParseExpression();
            if (element == nullptr) return nullptr;
            elements.push_back(std::move(element));
            if (elements.size() == 1 && Match(TokenType::SemiColon)) {
                const std::optional<std::uint64_t> count = ParseArrayLength();
                if (!count.has_value()) return nullptr;
                repeat = true;
                length = *count;
                break;
            }
            if (!Match(TokenType::Comma) || Check(TokenType::RBracket)) break;
        }
    }
    const Token* close =
        Consume(TokenType::RBracket, "']' after array initializer");
    if (close == nullptr) return nullptr;
    return std::make_unique<ArrayExpression>(
        source::SourceRange{start, close->range.end}, std::move(elements),
        repeat, length);
}

std::unique_ptr<Expression> Parser::ParsePrimaryExpression() {
    const Token& token = Current();
    switch (token.type) {
        case TokenType::LBracket:
            return ParseArrayExpression();
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
    while (!IsAtEnd() && !StartsType() && !Check(TokenType::Unsafe)) {
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
