#ifndef FLECHA_CORE_AST_HPP
#define FLECHA_CORE_AST_HPP

#include <cassert>
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
    DeferStatement,
    IfStatement,
    WhileStatement,
    RangeStatement,
    ProceedStatement,
    BreakStatement,
    JumpToStatement,
    PositionStatement,
    ConditionalBranch,
    AlsoClause,
    VariableDeclarationStatement,
    ExpressionStatement,
    ArrayExpression,
    LiteralExpression,
    NameExpression,
    ParenthesizedExpression,
    UnaryExpression,
    BinaryExpression,
    AssignmentExpression,
    CallExpression,
    MemberCallExpression,
    IndexExpression,
    SliceExpression,
    CastExpression,
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
    String,
    StringView,
};

enum class TypeKind { Primitive, Pointer, Reference, Array, Slice };

class TypeNode final : public AstNode {
   public:
    TypeNode(source::SourceRange range, PrimitiveType type,
             bool isConst = false, source::SourceRange qualifierRange = {})
        : AstNode(range),
          _typeKind(TypeKind::Primitive),
          _type(type),
          _isConst(isConst),
          _qualifierRange(qualifierRange) {}

    TypeNode(source::SourceRange range, TypeKind typeKind,
             std::unique_ptr<TypeNode> target, bool isConst = false,
             source::SourceRange qualifierRange = {}, std::uint64_t length = 0)
        : AstNode(range),
          _typeKind(typeKind),
          _length(length),
          _target(std::move(target)),
          _isConst(isConst),
          _qualifierRange(qualifierRange) {
        assert(typeKind != TypeKind::Primitive && _target != nullptr);
    }

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::Type;
    }
    [[nodiscard]] TypeKind TypeForm() const { return _typeKind; }
    [[nodiscard]] std::uint64_t Length() const { return _length; }
    [[nodiscard]] PrimitiveType Type() const {
        assert(_typeKind == TypeKind::Primitive);
        return _type;
    }
    [[nodiscard]] const TypeNode* Target() const { return _target.get(); }
    [[nodiscard]] bool IsConst() const { return _isConst; }
    [[nodiscard]] const source::SourceRange& QualifierRange() const {
        return _qualifierRange;
    }

   private:
    TypeKind _typeKind;
    std::uint64_t _length = 0;
    PrimitiveType _type = PrimitiveType::Void;
    std::unique_ptr<TypeNode> _target;
    bool _isConst;
    source::SourceRange _qualifierRange;
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
                   std::vector<std::unique_ptr<Statement>> statements,
                   bool isUnsafe = false)
        : Statement(range),
          _statements(std::move(statements)),
          _isUnsafe(isUnsafe) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::BlockStatement;
    }
    [[nodiscard]] bool IsUnsafe() const { return _isUnsafe; }
    [[nodiscard]] const std::vector<std::unique_ptr<Statement>>& Statements()
        const {
        return _statements;
    }

   private:
    std::vector<std::unique_ptr<Statement>> _statements;
    bool _isUnsafe;
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
    [[nodiscard]] bool IsUnsafe() const {
        for (FunctionModifier modifier : _modifiers) {
            if (modifier == FunctionModifier::Unsafe) return true;
        }
        return false;
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
                   std::unique_ptr<AlsoClause> alsoClause,
                   std::unique_ptr<Expression> update = nullptr)
        : Statement(range),
          _condition(std::move(condition)),
          _body(std::move(body)),
          _alsoClause(std::move(alsoClause)),
          _update(std::move(update)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::WhileStatement;
    }
    [[nodiscard]] const Expression& Condition() const { return *_condition; }
    [[nodiscard]] const BlockStatement& Body() const { return *_body; }
    [[nodiscard]] const AlsoClause* Also() const { return _alsoClause.get(); }
    [[nodiscard]] const Expression* Update() const { return _update.get(); }

   private:
    std::unique_ptr<Expression> _condition;
    std::unique_ptr<BlockStatement> _body;
    std::unique_ptr<AlsoClause> _alsoClause;
    std::unique_ptr<Expression> _update;
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
        : Statement(range), _name(std::move(name)), _nameRange(nameRange) {}
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
                                 source::SourceRange nameRange = {},
                                 bool isRangeSource = false)
        : Statement(range),
          _type(std::move(type)),
          _name(std::move(name)),
          _initializer(std::move(initializer)),
          _nameRange(nameRange.Empty() ? range : nameRange),
          _isRangeSource(isRangeSource) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::VariableDeclarationStatement;
    }
    [[nodiscard]] bool IsRangeSource() const { return _isRangeSource; }
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
    bool _isRangeSource;
};

class DeferStatement final : public Statement {
   public:
    DeferStatement(source::SourceRange range, std::unique_ptr<Expression> call)
        : Statement(range), _call(std::move(call)) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::DeferStatement;
    }
    [[nodiscard]] const Expression& Call() const { return *_call; }

   private:
    std::unique_ptr<Expression> _call;
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

class ArrayExpression final : public Expression {
   public:
    ArrayExpression(source::SourceRange range,
                    std::vector<std::unique_ptr<Expression>> elements,
                    bool repeat = false, std::uint64_t length = 0)
        : Expression(range),
          _elements(std::move(elements)),
          _repeat(repeat),
          _length(repeat ? length : _elements.size()) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::ArrayExpression;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& Elements()
        const {
        return _elements;
    }
    [[nodiscard]] bool IsRepeat() const { return _repeat; }
    [[nodiscard]] std::uint64_t Length() const { return _length; }

   private:
    std::vector<std::unique_ptr<Expression>> _elements;
    bool _repeat;
    std::uint64_t _length;
};

// Range syntax is normalized into an array snapshot, a private usize cursor,
// and a while loop. Retain the source/binding metadata for range-specific
// checks.
class RangeStatement final : public Statement {
   public:
    RangeStatement(source::SourceRange range,
                   std::unique_ptr<BlockStatement> expansion,
                   const VariableDeclarationStatement* source,
                   const VariableDeclarationStatement* indexBinding)
        : Statement(range),
          _expansion(std::move(expansion)),
          _source(source),
          _indexBinding(indexBinding) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::RangeStatement;
    }
    [[nodiscard]] const BlockStatement& Expansion() const {
        return *_expansion;
    }
    [[nodiscard]] const VariableDeclarationStatement& Source() const {
        return *_source;
    }
    [[nodiscard]] const VariableDeclarationStatement* IndexBinding() const {
        return _indexBinding;
    }

   private:
    std::unique_ptr<BlockStatement> _expansion;
    const VariableDeclarationStatement* _source;
    const VariableDeclarationStatement* _indexBinding;
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

class CastExpression final : public Expression {
   public:
    CastExpression(source::SourceRange range,
                   std::unique_ptr<Expression> expression,
                   std::unique_ptr<TypeNode> targetType)
        : Expression(range),
          _expression(std::move(expression)),
          _targetType(std::move(targetType)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::CastExpression;
    }
    [[nodiscard]] const Expression& Value() const { return *_expression; }
    [[nodiscard]] const TypeNode& TargetType() const { return *_targetType; }

   private:
    std::unique_ptr<Expression> _expression;
    std::unique_ptr<TypeNode> _targetType;
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

class MemberCallExpression final : public Expression {
   public:
    MemberCallExpression(source::SourceRange range,
                         std::unique_ptr<Expression> receiver,
                         std::string member, source::SourceRange memberRange,
                         std::vector<std::unique_ptr<Expression>> arguments)
        : Expression(range),
          _receiver(std::move(receiver)),
          _member(std::move(member)),
          _memberRange(memberRange),
          _arguments(std::move(arguments)) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::MemberCallExpression;
    }
    [[nodiscard]] const Expression& Receiver() const { return *_receiver; }
    [[nodiscard]] const std::string& Member() const { return _member; }
    [[nodiscard]] const source::SourceRange& MemberRange() const {
        return _memberRange;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Expression>>& Arguments()
        const {
        return _arguments;
    }

   private:
    std::unique_ptr<Expression> _receiver;
    std::string _member;
    source::SourceRange _memberRange;
    std::vector<std::unique_ptr<Expression>> _arguments;
};

class SliceExpression final : public Expression {
   public:
    SliceExpression(source::SourceRange range,
                    std::unique_ptr<Expression> receiver,
                    std::unique_ptr<Expression> start,
                    std::unique_ptr<Expression> end)
        : Expression(range),
          _receiver(std::move(receiver)),
          _start(std::move(start)),
          _end(std::move(end)) {}
    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::SliceExpression;
    }
    [[nodiscard]] const Expression& Receiver() const { return *_receiver; }
    [[nodiscard]] const Expression* Start() const { return _start.get(); }
    [[nodiscard]] const Expression* End() const { return _end.get(); }

   private:
    std::unique_ptr<Expression> _receiver;
    std::unique_ptr<Expression> _start;
    std::unique_ptr<Expression> _end;
};

class IndexExpression final : public Expression {
   public:
    IndexExpression(source::SourceRange range,
                    std::unique_ptr<Expression> receiver,
                    std::unique_ptr<Expression> index,
                    bool isRangeElement = false)
        : Expression(range),
          _receiver(std::move(receiver)),
          _index(std::move(index)),
          _isRangeElement(isRangeElement) {}

    [[nodiscard]] AstNodeKind Kind() const override {
        return AstNodeKind::IndexExpression;
    }
    [[nodiscard]] const Expression& Receiver() const { return *_receiver; }
    [[nodiscard]] const Expression& Index() const { return *_index; }
    [[nodiscard]] bool IsRangeElement() const { return _isRangeElement; }

   private:
    std::unique_ptr<Expression> _receiver;
    std::unique_ptr<Expression> _index;
    bool _isRangeElement;
};

}  // namespace flecha::core

#endif  // FLECHA_CORE_AST_HPP
