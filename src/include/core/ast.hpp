#ifndef FLECHA_CORE_AST_HPP
#define FLECHA_CORE_AST_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "source/source_range.hpp"
#include "token_type.hpp"

namespace flecha::core {

enum class AstNodeKind {
    CompilationUnit,
    FunctionDeclaration,
    Parameter,
    Type,
    BlockStatement,
    ReturnStatement,
    IfStatement,
    WhileStatement,
    ProceedStatement,
    BreakStatement,
    JumpToStatement,
    PositionStatement,
    ConditionalBranch,
    AlsoClause,
    VariableDeclarationStatement,
    ExpressionStatement,
    LiteralExpression,
    NameExpression,
    ParenthesizedExpression,
    UnaryExpression,
    BinaryExpression,
    AssignmentExpression,
    CallExpression,
};

class AstNode {
   public:
    explicit AstNode(source::SourceRange range) : _range(range) {}
    virtual ~AstNode() = default;

    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
    AstNode(AstNode&&) = default;
    AstNode& operator=(AstNode&&) = default;

    [[nodiscard]] virtual AstNodeKind Kind() const = 0;
    [[nodiscard]] const source::SourceRange& Range() const { return _range; }

   private:
    source::SourceRange _range;
};

class Declaration : public AstNode {
   public:
    using AstNode::AstNode;
};

class Statement : public AstNode {
   public:
    using AstNode::AstNode;
};

class Expression : public AstNode {
   public:
    using AstNode::AstNode;
};

enum class PrimitiveType {
    I8,
    I16,
    I32,
    I64,
    ISize,
    U8,
    U16,
    U32,
    U64,
    USize,
    F32,
    F64,
    Bool,
    Char,
    Void,
};

class TypeNode final : public AstNode {
   public:
    TypeNode(source::SourceRange range, PrimitiveType type)
        : AstNode(range), _type(type) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::Type;
    }
    [[nodiscard]] PrimitiveType Type() const { return _type; }

   private:
    PrimitiveType _type;
};

class Parameter final : public AstNode {
   public:
    Parameter(source::SourceRange range, std::unique_ptr<TypeNode> type,
              std::string name, source::SourceRange nameRange = {})
        : AstNode(range),
          _type(std::move(type)),
          _name(std::move(name)),
          _nameRange(nameRange.Empty() ? range : nameRange) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::Parameter;
    }
    [[nodiscard]] const TypeNode& Type() const { return *_type; }
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] const source::SourceRange& NameRange() const {
        return _nameRange;
    }

   private:
    std::unique_ptr<TypeNode> _type;
    std::string _name;
    source::SourceRange _nameRange;
};

enum class FunctionModifier : std::uint8_t {
    Unsafe,
    Export,
    Extern,
    Inline,
    Comptime,
};

class BlockStatement final : public Statement {
   public:
    BlockStatement(source::SourceRange range,
                   std::vector<std::unique_ptr<Statement>> statements)
        : Statement(range), _statements(std::move(statements)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::BlockStatement;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Statement>>& Statements()
        const {
        return _statements;
    }

   private:
    std::vector<std::unique_ptr<Statement>> _statements;
};

class FunctionDeclaration final : public Declaration {
   public:
    FunctionDeclaration(source::SourceRange range,
                        std::vector<FunctionModifier> modifiers,
                        std::unique_ptr<TypeNode> returnType, std::string name,
                        std::vector<std::unique_ptr<Parameter>> parameters,
                        std::unique_ptr<BlockStatement> body,
                        source::SourceRange nameRange = {})
        : Declaration(range),
          _modifiers(std::move(modifiers)),
          _returnType(std::move(returnType)),
          _name(std::move(name)),
          _parameters(std::move(parameters)),
          _body(std::move(body)),
          _nameRange(nameRange.Empty() ? range : nameRange) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::FunctionDeclaration;
    }
    [[nodiscard]] const std::vector<FunctionModifier>& Modifiers() const {
        return _modifiers;
    }
    [[nodiscard]] const TypeNode& ReturnType() const { return *_returnType; }
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] const source::SourceRange& NameRange() const {
        return _nameRange;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Parameter>>& Parameters()
        const {
        return _parameters;
    }
    [[nodiscard]] const BlockStatement& Body() const { return *_body; }

   private:
    std::vector<FunctionModifier> _modifiers;
    std::unique_ptr<TypeNode> _returnType;
    std::string _name;
    std::vector<std::unique_ptr<Parameter>> _parameters;
    std::unique_ptr<BlockStatement> _body;
    source::SourceRange _nameRange;
};

class CompilationUnit final : public AstNode {
   public:
    CompilationUnit(source::SourceRange range,
                    std::vector<std::unique_ptr<Declaration>> declarations)
        : AstNode(range), _declarations(std::move(declarations)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::CompilationUnit;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Declaration>>&
    Declarations() const {
        return _declarations;
    }

   private:
    std::vector<std::unique_ptr<Declaration>> _declarations;
};

class ReturnStatement final : public Statement {
   public:
    ReturnStatement(source::SourceRange range,
                    std::unique_ptr<Expression> expression)
        : Statement(range), _expression(std::move(expression)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ReturnStatement;
    }
    [[nodiscard]] const Expression* Value() const { return _expression.get(); }

   private:
    std::unique_ptr<Expression> _expression;
};

class AlsoClause final : public AstNode {
   public:
    AlsoClause(source::SourceRange range, std::unique_ptr<Expression> condition,
               std::unique_ptr<BlockStatement> body)
        : AstNode(range),
          _condition(std::move(condition)),
          _body(std::move(body)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::AlsoClause;
    }
    [[nodiscard]] const Expression& Condition() const { return *_condition; }
    [[nodiscard]] const BlockStatement& Body() const { return *_body; }

   private:
    std::unique_ptr<Expression> _condition;
    std::unique_ptr<BlockStatement> _body;
};

class ConditionalBranch final : public AstNode {
   public:
    ConditionalBranch(source::SourceRange range,
                      std::unique_ptr<Expression> condition,
                      std::unique_ptr<BlockStatement> body,
                      std::unique_ptr<AlsoClause> alsoClause)
        : AstNode(range),
          _condition(std::move(condition)),
          _body(std::move(body)),
          _alsoClause(std::move(alsoClause)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ConditionalBranch;
    }
    [[nodiscard]] const Expression& Condition() const { return *_condition; }
    [[nodiscard]] const BlockStatement& Body() const { return *_body; }
    [[nodiscard]] const AlsoClause* Also() const { return _alsoClause.get(); }

   private:
    std::unique_ptr<Expression> _condition;
    std::unique_ptr<BlockStatement> _body;
    std::unique_ptr<AlsoClause> _alsoClause;
};

class IfStatement final : public Statement {
   public:
    IfStatement(source::SourceRange range,
                std::unique_ptr<ConditionalBranch> primaryBranch,
                std::vector<std::unique_ptr<ConditionalBranch>> elseIfBranches,
                std::unique_ptr<BlockStatement> elseBody)
        : Statement(range),
          _primaryBranch(std::move(primaryBranch)),
          _elseIfBranches(std::move(elseIfBranches)),
          _elseBody(std::move(elseBody)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::IfStatement;
    }
    [[nodiscard]] const ConditionalBranch& PrimaryBranch() const {
        return *_primaryBranch;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<ConditionalBranch>>&
    ElseIfBranches() const {
        return _elseIfBranches;
    }
    [[nodiscard]] const BlockStatement* ElseBody() const {
        return _elseBody.get();
    }

   private:
    std::unique_ptr<ConditionalBranch> _primaryBranch;
    std::vector<std::unique_ptr<ConditionalBranch>> _elseIfBranches;
    std::unique_ptr<BlockStatement> _elseBody;
};

class WhileStatement final : public Statement {
   public:
    WhileStatement(source::SourceRange range,
                   std::unique_ptr<Expression> condition,
                   std::unique_ptr<BlockStatement> body,
                   std::unique_ptr<AlsoClause> alsoClause)
        : Statement(range),
          _condition(std::move(condition)),
          _body(std::move(body)),
          _alsoClause(std::move(alsoClause)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::WhileStatement;
    }
    [[nodiscard]] const Expression& Condition() const { return *_condition; }
    [[nodiscard]] const BlockStatement& Body() const { return *_body; }
    [[nodiscard]] const AlsoClause* Also() const { return _alsoClause.get(); }

   private:
    std::unique_ptr<Expression> _condition;
    std::unique_ptr<BlockStatement> _body;
    std::unique_ptr<AlsoClause> _alsoClause;
};

class ProceedStatement final : public Statement {
   public:
    using Statement::Statement;
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ProceedStatement;
    }
};

class BreakStatement final : public Statement {
   public:
    using Statement::Statement;
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::BreakStatement;
    }
};

class JumpToStatement final : public Statement {
   public:
    JumpToStatement(source::SourceRange range, std::string position,
                    source::SourceRange positionRange)
        : Statement(range),
          _position(std::move(position)),
          _positionRange(positionRange) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::JumpToStatement;
    }
    [[nodiscard]] const std::string& Position() const { return _position; }
    [[nodiscard]] const source::SourceRange& PositionRange() const {
        return _positionRange;
    }

   private:
    std::string _position;
    source::SourceRange _positionRange;
};

class PositionStatement final : public Statement {
   public:
    PositionStatement(source::SourceRange range, std::string name,
                      source::SourceRange nameRange)
        : Statement(range),
          _name(std::move(name)),
          _nameRange(nameRange) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::PositionStatement;
    }
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] const source::SourceRange& NameRange() const {
        return _nameRange;
    }

   private:
    std::string _name;
    source::SourceRange _nameRange;
};

class VariableDeclarationStatement final : public Statement {
   public:
    VariableDeclarationStatement(source::SourceRange range,
                                 std::unique_ptr<TypeNode> type,
                                 std::string name,
                                 std::unique_ptr<Expression> initializer,
                                 source::SourceRange nameRange = {})
        : Statement(range),
          _type(std::move(type)),
          _name(std::move(name)),
          _initializer(std::move(initializer)),
          _nameRange(nameRange.Empty() ? range : nameRange) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::VariableDeclarationStatement;
    }
    [[nodiscard]] const TypeNode* ExplicitType() const { return _type.get(); }
    [[nodiscard]] bool HasExplicitType() const { return _type != nullptr; }
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] const source::SourceRange& NameRange() const {
        return _nameRange;
    }
    [[nodiscard]] const Expression* Initializer() const {
        return _initializer.get();
    }

   private:
    std::unique_ptr<TypeNode> _type;
    std::string _name;
    std::unique_ptr<Expression> _initializer;
    source::SourceRange _nameRange;
};

class ExpressionStatement final : public Statement {
   public:
    ExpressionStatement(source::SourceRange range,
                        std::unique_ptr<Expression> expression)
        : Statement(range), _expression(std::move(expression)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ExpressionStatement;
    }
    [[nodiscard]] const Expression& Value() const { return *_expression; }

   private:
    std::unique_ptr<Expression> _expression;
};

class LiteralExpression final : public Expression {
   public:
    LiteralExpression(source::SourceRange range, TokenType literalType,
                      std::string value)
        : Expression(range),
          _literalType(literalType),
          _value(std::move(value)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::LiteralExpression;
    }
    [[nodiscard]] TokenType LiteralType() const { return _literalType; }
    [[nodiscard]] const std::string& Value() const { return _value; }

   private:
    TokenType _literalType;
    std::string _value;
};

class NameExpression final : public Expression {
   public:
    NameExpression(source::SourceRange range, std::string name)
        : Expression(range), _name(std::move(name)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::NameExpression;
    }
    [[nodiscard]] const std::string& Name() const { return _name; }

   private:
    std::string _name;
};

class ParenthesizedExpression final : public Expression {
   public:
    ParenthesizedExpression(source::SourceRange range,
                            std::unique_ptr<Expression> expression)
        : Expression(range), _expression(std::move(expression)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ParenthesizedExpression;
    }
    [[nodiscard]] const Expression& Inner() const { return *_expression; }

   private:
    std::unique_ptr<Expression> _expression;
};

class UnaryExpression final : public Expression {
   public:
    UnaryExpression(source::SourceRange range, TokenType operatorType,
                    std::unique_ptr<Expression> operand)
        : Expression(range),
          _operatorType(operatorType),
          _operand(std::move(operand)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::UnaryExpression;
    }
    [[nodiscard]] TokenType OperatorType() const { return _operatorType; }
    [[nodiscard]] const Expression& Operand() const { return *_operand; }

   private:
    TokenType _operatorType;
    std::unique_ptr<Expression> _operand;
};

class BinaryExpression final : public Expression {
   public:
    BinaryExpression(source::SourceRange range,
                     std::unique_ptr<Expression> left, TokenType operatorType,
                     std::unique_ptr<Expression> right)
        : Expression(range),
          _left(std::move(left)),
          _operatorType(operatorType),
          _right(std::move(right)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::BinaryExpression;
    }
    [[nodiscard]] const Expression& Left() const { return *_left; }
    [[nodiscard]] TokenType OperatorType() const { return _operatorType; }
    [[nodiscard]] const Expression& Right() const { return *_right; }

   private:
    std::unique_ptr<Expression> _left;
    TokenType _operatorType;
    std::unique_ptr<Expression> _right;
};

class AssignmentExpression final : public Expression {
   public:
    AssignmentExpression(source::SourceRange range,
                         std::unique_ptr<Expression> target,
                         std::unique_ptr<Expression> value)
        : Expression(range),
          _target(std::move(target)),
          _value(std::move(value)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::AssignmentExpression;
    }
    [[nodiscard]] const Expression& Target() const { return *_target; }
    [[nodiscard]] const Expression& Value() const { return *_value; }

   private:
    std::unique_ptr<Expression> _target;
    std::unique_ptr<Expression> _value;
};

class CallExpression final : public Expression {
   public:
    CallExpression(source::SourceRange range,
                   std::unique_ptr<Expression> callee,
                   std::vector<std::unique_ptr<Expression>> arguments)
        : Expression(range),
          _callee(std::move(callee)),
          _arguments(std::move(arguments)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::CallExpression;
    }
    [[nodiscard]] const Expression& Callee() const { return *_callee; }
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& Arguments()
        const {
        return _arguments;
    }

   private:
    std::unique_ptr<Expression> _callee;
    std::vector<std::unique_ptr<Expression>> _arguments;
};

}  // namespace flecha::core

#endif  // FLECHA_CORE_AST_HPP
