#ifndef FLECHA_LOWERING_BOUND_IR_HPP
#define FLECHA_LOWERING_BOUND_IR_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/token_type.hpp"
#include "semantic/symbol.hpp"
#include "source/source_range.hpp"

namespace flecha::lowering {

enum class BoundNodeKind {
    BlockStatement,
    VariableDeclarationStatement,
    ExpressionStatement,
    ReturnStatement,
    IfStatement,
    WhileStatement,
    ProceedStatement,
    BreakStatement,
    JumpToStatement,
    PositionStatement,
    LiteralExpression,
    VariableExpression,
    FunctionExpression,
    UnaryExpression,
    BinaryExpression,
    AssignmentExpression,
    CallExpression,
    StringViewOperationExpression,
    CastExpression,
};

enum class StringViewOperation { Length, At, Substring };

class BoundNode {
   public:
    explicit BoundNode(source::SourceRange range) : _range(range) {}
    virtual ~BoundNode() = default;
    BoundNode(const BoundNode&) = delete;
    BoundNode& operator=(const BoundNode&) = delete;
    BoundNode(BoundNode&&) = default;
    BoundNode& operator=(BoundNode&&) = default;

    [[nodiscard]] virtual BoundNodeKind Kind() const = 0;
    [[nodiscard]] const source::SourceRange& Range() const { return _range; }

   private:
    source::SourceRange _range;
};

class BoundExpression : public BoundNode {
   public:
    BoundExpression(source::SourceRange range, semantic::SemanticType type)
        : BoundNode(range), _type(type) {}
    [[nodiscard]] semantic::SemanticType Type() const { return _type; }

   private:
    semantic::SemanticType _type;
};

class BoundStatement : public BoundNode {
   public:
    using BoundNode::BoundNode;
};

class BoundLiteralExpression final : public BoundExpression {
   public:
    BoundLiteralExpression(source::SourceRange range,
                           semantic::SemanticType type, std::string value)
        : BoundExpression(range, type), _value(std::move(value)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::LiteralExpression;
    }
    [[nodiscard]] const std::string& Value() const { return _value; }

   private:
    std::string _value;
};

class BoundVariableExpression final : public BoundExpression {
   public:
    BoundVariableExpression(source::SourceRange range,
                            const semantic::VariableSymbol& symbol)
        : BoundExpression(range, symbol.Type()), _symbol(symbol) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::VariableExpression;
    }
    [[nodiscard]] const semantic::VariableSymbol& Symbol() const {
        return _symbol;
    }

   private:
    const semantic::VariableSymbol& _symbol;
};

class BoundFunctionExpression final : public BoundExpression {
   public:
    BoundFunctionExpression(source::SourceRange range,
                            const semantic::FunctionSymbol& symbol)
        : BoundExpression(range, symbol.Type()), _symbol(symbol) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::FunctionExpression;
    }
    [[nodiscard]] const semantic::FunctionSymbol& Symbol() const {
        return _symbol;
    }

   private:
    const semantic::FunctionSymbol& _symbol;
};

class BoundUnaryExpression final : public BoundExpression {
   public:
    BoundUnaryExpression(source::SourceRange range, semantic::SemanticType type,
                         TokenType operatorType,
                         std::unique_ptr<BoundExpression> operand)
        : BoundExpression(range, type),
          _operatorType(operatorType),
          _operand(std::move(operand)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::UnaryExpression;
    }
    [[nodiscard]] TokenType OperatorType() const { return _operatorType; }
    [[nodiscard]] const BoundExpression& Operand() const { return *_operand; }

   private:
    TokenType _operatorType;
    std::unique_ptr<BoundExpression> _operand;
};

class BoundCastExpression final : public BoundExpression {
   public:
    BoundCastExpression(source::SourceRange range,
                        semantic::SemanticType targetType,
                        std::unique_ptr<BoundExpression> expression)
        : BoundExpression(range, targetType),
          _expression(std::move(expression)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::CastExpression;
    }
    [[nodiscard]] const BoundExpression& Value() const { return *_expression; }

   private:
    std::unique_ptr<BoundExpression> _expression;
};

class BoundBinaryExpression final : public BoundExpression {
   public:
    BoundBinaryExpression(source::SourceRange range,
                          semantic::SemanticType type,
                          std::unique_ptr<BoundExpression> left,
                          TokenType operatorType,
                          std::unique_ptr<BoundExpression> right)
        : BoundExpression(range, type),
          _left(std::move(left)),
          _operatorType(operatorType),
          _right(std::move(right)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::BinaryExpression;
    }
    [[nodiscard]] const BoundExpression& Left() const { return *_left; }
    [[nodiscard]] TokenType OperatorType() const { return _operatorType; }
    [[nodiscard]] const BoundExpression& Right() const { return *_right; }

   private:
    std::unique_ptr<BoundExpression> _left;
    TokenType _operatorType;
    std::unique_ptr<BoundExpression> _right;
};

class BoundAssignmentExpression final : public BoundExpression {
   public:
    BoundAssignmentExpression(source::SourceRange range,
                              const semantic::VariableSymbol& target,
                              std::unique_ptr<BoundExpression> value)
        : BoundExpression(range, target.Type()),
          _target(target),
          _value(std::move(value)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::AssignmentExpression;
    }
    [[nodiscard]] const semantic::VariableSymbol& Target() const {
        return _target;
    }
    [[nodiscard]] const BoundExpression& Value() const { return *_value; }

   private:
    const semantic::VariableSymbol& _target;
    std::unique_ptr<BoundExpression> _value;
};

class BoundCallExpression final : public BoundExpression {
   public:
    BoundCallExpression(source::SourceRange range,
                        const semantic::FunctionSymbol& function,
                        std::vector<std::unique_ptr<BoundExpression>> arguments)
        : BoundExpression(range, function.ReturnType()),
          _function(function),
          _arguments(std::move(arguments)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::CallExpression;
    }
    [[nodiscard]] const semantic::FunctionSymbol& Function() const {
        return _function;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<BoundExpression>>&
    Arguments() const {
        return _arguments;
    }

   private:
    const semantic::FunctionSymbol& _function;
    std::vector<std::unique_ptr<BoundExpression>> _arguments;
};

class BoundStringViewOperationExpression final : public BoundExpression {
   public:
    BoundStringViewOperationExpression(
        source::SourceRange range, semantic::SemanticType type,
        StringViewOperation operation,
        std::unique_ptr<BoundExpression> receiver,
        std::vector<std::unique_ptr<BoundExpression>> arguments)
        : BoundExpression(range, type),
          _operation(operation),
          _receiver(std::move(receiver)),
          _arguments(std::move(arguments)) {}

    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::StringViewOperationExpression;
    }
    [[nodiscard]] StringViewOperation Operation() const { return _operation; }
    [[nodiscard]] const BoundExpression& Receiver() const { return *_receiver; }
    [[nodiscard]] const std::vector<std::unique_ptr<BoundExpression>>&
    Arguments() const {
        return _arguments;
    }

   private:
    StringViewOperation _operation;
    std::unique_ptr<BoundExpression> _receiver;
    std::vector<std::unique_ptr<BoundExpression>> _arguments;
};

class BoundBlockStatement final : public BoundStatement {
   public:
    BoundBlockStatement(source::SourceRange range,
                        std::vector<std::unique_ptr<BoundStatement>> statements)
        : BoundStatement(range), _statements(std::move(statements)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::BlockStatement;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<BoundStatement>>&
    Statements() const {
        return _statements;
    }

   private:
    std::vector<std::unique_ptr<BoundStatement>> _statements;
};

class BoundVariableDeclarationStatement final : public BoundStatement {
   public:
    BoundVariableDeclarationStatement(
        source::SourceRange range, const semantic::VariableSymbol& variable,
        std::unique_ptr<BoundExpression> initializer)
        : BoundStatement(range),
          _variable(variable),
          _initializer(std::move(initializer)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::VariableDeclarationStatement;
    }
    [[nodiscard]] const semantic::VariableSymbol& Variable() const {
        return _variable;
    }
    [[nodiscard]] const BoundExpression& Initializer() const {
        return *_initializer;
    }

   private:
    const semantic::VariableSymbol& _variable;
    std::unique_ptr<BoundExpression> _initializer;
};

class BoundExpressionStatement final : public BoundStatement {
   public:
    BoundExpressionStatement(source::SourceRange range,
                             std::unique_ptr<BoundExpression> expression)
        : BoundStatement(range), _expression(std::move(expression)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::ExpressionStatement;
    }
    [[nodiscard]] const BoundExpression& Expression() const {
        return *_expression;
    }

   private:
    std::unique_ptr<BoundExpression> _expression;
};

class BoundReturnStatement final : public BoundStatement {
   public:
    BoundReturnStatement(source::SourceRange range,
                         std::unique_ptr<BoundExpression> expression)
        : BoundStatement(range), _expression(std::move(expression)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::ReturnStatement;
    }
    [[nodiscard]] const BoundExpression* Expression() const {
        return _expression.get();
    }

   private:
    std::unique_ptr<BoundExpression> _expression;
};

class BoundIfStatement final : public BoundStatement {
   public:
    BoundIfStatement(source::SourceRange range,
                     std::unique_ptr<BoundExpression> condition,
                     std::unique_ptr<BoundBlockStatement> thenBlock,
                     std::unique_ptr<BoundBlockStatement> elseBlock)
        : BoundStatement(range),
          _condition(std::move(condition)),
          _thenBlock(std::move(thenBlock)),
          _elseBlock(std::move(elseBlock)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::IfStatement;
    }
    [[nodiscard]] const BoundExpression& Condition() const {
        return *_condition;
    }
    [[nodiscard]] const BoundBlockStatement& ThenBlock() const {
        return *_thenBlock;
    }
    [[nodiscard]] const BoundBlockStatement* ElseBlock() const {
        return _elseBlock.get();
    }

   private:
    std::unique_ptr<BoundExpression> _condition;
    std::unique_ptr<BoundBlockStatement> _thenBlock;
    std::unique_ptr<BoundBlockStatement> _elseBlock;
};

class BoundWhileStatement final : public BoundStatement {
   public:
    BoundWhileStatement(source::SourceRange range,
                        std::unique_ptr<BoundExpression> condition,
                        std::unique_ptr<BoundBlockStatement> body)
        : BoundStatement(range),
          _condition(std::move(condition)),
          _body(std::move(body)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::WhileStatement;
    }
    [[nodiscard]] const BoundExpression& Condition() const {
        return *_condition;
    }
    [[nodiscard]] const BoundBlockStatement& Body() const { return *_body; }

   private:
    std::unique_ptr<BoundExpression> _condition;
    std::unique_ptr<BoundBlockStatement> _body;
};

class BoundProceedStatement final : public BoundStatement {
   public:
    using BoundStatement::BoundStatement;
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::ProceedStatement;
    }
};

class BoundBreakStatement final : public BoundStatement {
   public:
    using BoundStatement::BoundStatement;
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::BreakStatement;
    }
};

class BoundJumpToStatement final : public BoundStatement {
   public:
    BoundJumpToStatement(source::SourceRange range, std::string position)
        : BoundStatement(range), _position(std::move(position)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::JumpToStatement;
    }
    [[nodiscard]] const std::string& Position() const { return _position; }

   private:
    std::string _position;
};

class BoundPositionStatement final : public BoundStatement {
   public:
    BoundPositionStatement(source::SourceRange range, std::string name)
        : BoundStatement(range), _name(std::move(name)) {}
    [[nodiscard]] BoundNodeKind Kind() const override {
        return BoundNodeKind::PositionStatement;
    }
    [[nodiscard]] const std::string& Name() const { return _name; }

   private:
    std::string _name;
};

class BoundFunction final {
   public:
    BoundFunction(const semantic::FunctionSymbol& symbol,
                  std::vector<const semantic::VariableSymbol*> parameters,
                  std::unique_ptr<BoundBlockStatement> body)
        : _symbol(symbol),
          _parameters(std::move(parameters)),
          _body(std::move(body)) {}
    [[nodiscard]] const semantic::FunctionSymbol& Symbol() const {
        return _symbol;
    }
    [[nodiscard]] const std::vector<const semantic::VariableSymbol*>&
    Parameters() const {
        return _parameters;
    }
    [[nodiscard]] const BoundBlockStatement& Body() const { return *_body; }

   private:
    const semantic::FunctionSymbol& _symbol;
    std::vector<const semantic::VariableSymbol*> _parameters;
    std::unique_ptr<BoundBlockStatement> _body;
};

class BoundProgram final {
   public:
    explicit BoundProgram(std::vector<std::unique_ptr<BoundFunction>> functions)
        : _functions(std::move(functions)) {}
    BoundProgram(const BoundProgram&) = delete;
    BoundProgram& operator=(const BoundProgram&) = delete;
    BoundProgram(BoundProgram&&) = default;
    BoundProgram& operator=(BoundProgram&&) = default;
    [[nodiscard]] const std::vector<std::unique_ptr<BoundFunction>>& Functions()
        const {
        return _functions;
    }

   private:
    std::vector<std::unique_ptr<BoundFunction>> _functions;
};

}  // namespace flecha::lowering

#endif  // FLECHA_LOWERING_BOUND_IR_HPP
