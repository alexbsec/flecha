#include "lowering/bound_ir_dumper.hpp"

#include <sstream>
#include <string>

namespace flecha::lowering {
namespace {

std::string OperatorText(TokenType type) {
    switch (type) {
        case TokenType::Assign:
            return "=";
        case TokenType::Add:
            return "+";
        case TokenType::Sub:
            return "-";
        case TokenType::Mul:
            return "*";
        case TokenType::Div:
            return "/";
        case TokenType::Mod:
            return "%";
        case TokenType::Xor:
            return "^";
        case TokenType::Bang:
            return "!";
        case TokenType::Equal:
            return "==";
        case TokenType::NotEqual:
            return "!=";
        case TokenType::StrictEqual:
            return "===";
        case TokenType::StrictNotEqual:
            return "!==";
        case TokenType::And:
            return "and";
        case TokenType::Or:
            return "or";
        case TokenType::Less:
            return "<";
        case TokenType::LessEqual:
            return "<=";
        case TokenType::Greater:
            return ">";
        case TokenType::GreaterEqual:
            return ">=";
        default:
            return "?";
    }
}

class Dumper final {
   public:
    std::string Dump(const BoundProgram& program) {
        for (const std::unique_ptr<BoundFunction>& function :
             program.Functions()) {
            WriteFunction(*function);
        }
        return _stream.str();
    }

   private:
    void Indent() {
        for (int index = 0; index < _indent; ++index) _stream << "  ";
    }

    void WriteFunction(const BoundFunction& function) {
        _stream << "function " << function.Symbol().Name() << "(";
        for (std::size_t index = 0; index < function.Parameters().size();
             ++index) {
            if (index > 0) _stream << ", ";
            const semantic::VariableSymbol& parameter =
                *function.Parameters()[index];
            _stream << parameter.Type().Name() << " " << parameter.Name();
        }
        _stream << ") -> " << function.Symbol().ReturnType().Name() << "\n";
        WriteBlock(function.Body());
    }

    void WriteBlock(const BoundBlockStatement& block) {
        Indent();
        _stream << "{\n";
        ++_indent;
        for (const std::unique_ptr<BoundStatement>& statement :
             block.Statements()) {
            WriteStatement(*statement);
        }
        --_indent;
        Indent();
        _stream << "}\n";
    }

    void WriteStatement(const BoundStatement& statement) {
        Indent();
        switch (statement.Kind()) {
            case BoundNodeKind::VariableDeclarationStatement: {
                const BoundVariableDeclarationStatement& declaration =
                    static_cast<const BoundVariableDeclarationStatement&>(
                        statement);
                _stream << "local " << declaration.Variable().Name() << ": "
                        << declaration.Variable().Type().Name() << " = ";
                WriteExpression(declaration.Initializer());
                _stream << "\n";
                break;
            }
            case BoundNodeKind::ExpressionStatement:
                WriteExpression(
                    static_cast<const BoundExpressionStatement&>(statement)
                        .Expression());
                _stream << "\n";
                break;
            case BoundNodeKind::ReturnStatement: {
                const BoundReturnStatement& returnStatement =
                    static_cast<const BoundReturnStatement&>(statement);
                _stream << "return";
                if (returnStatement.Expression() != nullptr) {
                    _stream << " ";
                    WriteExpression(*returnStatement.Expression());
                }
                _stream << "\n";
                break;
            }
            case BoundNodeKind::IfStatement: {
                const BoundIfStatement& ifStatement =
                    static_cast<const BoundIfStatement&>(statement);
                _stream << "if ";
                WriteExpression(ifStatement.Condition());
                _stream << "\n";
                WriteBlock(ifStatement.ThenBlock());
                if (ifStatement.ElseBlock() != nullptr) {
                    Indent();
                    _stream << "else\n";
                    WriteBlock(*ifStatement.ElseBlock());
                }
                break;
            }
            case BoundNodeKind::WhileStatement: {
                const BoundWhileStatement& whileStatement =
                    static_cast<const BoundWhileStatement&>(statement);
                _stream << "while ";
                WriteExpression(whileStatement.Condition());
                _stream << "\n";
                WriteBlock(whileStatement.Body());
                break;
            }
            case BoundNodeKind::ProceedStatement:
                _stream << "proceed\n";
                break;
            case BoundNodeKind::BreakStatement:
                _stream << "break\n";
                break;
            case BoundNodeKind::JumpToStatement:
                _stream << "jumpto "
                        << static_cast<const BoundJumpToStatement&>(statement)
                               .Position()
                        << "\n";
                break;
            case BoundNodeKind::PositionStatement:
                _stream << "position "
                        << static_cast<const BoundPositionStatement&>(statement)
                               .Name()
                        << "\n";
                break;
            default:
                break;
        }
    }

    void WriteExpression(const BoundExpression& expression) {
        switch (expression.Kind()) {
            case BoundNodeKind::LiteralExpression:
                _stream << static_cast<const BoundLiteralExpression&>(
                               expression)
                               .Value()
                        << ":" << expression.Type().Name();
                break;
            case BoundNodeKind::VariableExpression:
                _stream << "load "
                        << static_cast<const BoundVariableExpression&>(
                               expression)
                               .Symbol()
                               .Name();
                break;
            case BoundNodeKind::FunctionExpression:
                _stream << "function_ref "
                        << static_cast<const BoundFunctionExpression&>(
                               expression)
                               .Symbol()
                               .Name();
                break;
            case BoundNodeKind::UnaryExpression: {
                const BoundUnaryExpression& unary =
                    static_cast<const BoundUnaryExpression&>(expression);
                _stream << "(" << OperatorText(unary.OperatorType()) << " ";
                WriteExpression(unary.Operand());
                _stream << ")";
                break;
            }
            case BoundNodeKind::CastExpression: {
                const BoundCastExpression& cast =
                    static_cast<const BoundCastExpression&>(expression);
                _stream << "(cast " << cast.Type().Name() << " ";
                WriteExpression(cast.Value());
                _stream << ")";
                break;
            }
            case BoundNodeKind::BinaryExpression: {
                const BoundBinaryExpression& binary =
                    static_cast<const BoundBinaryExpression&>(expression);
                _stream << "(" << OperatorText(binary.OperatorType()) << " ";
                WriteExpression(binary.Left());
                _stream << " ";
                WriteExpression(binary.Right());
                _stream << ")";
                break;
            }
            case BoundNodeKind::AssignmentExpression: {
                const BoundAssignmentExpression& assignment =
                    static_cast<const BoundAssignmentExpression&>(expression);
                _stream << "(store " << assignment.Target().Name() << " ";
                WriteExpression(assignment.Value());
                _stream << ")";
                break;
            }
            case BoundNodeKind::CallExpression: {
                const BoundCallExpression& call =
                    static_cast<const BoundCallExpression&>(expression);
                _stream << "(call " << call.Function().Name();
                for (const std::unique_ptr<BoundExpression>& argument :
                     call.Arguments()) {
                    _stream << " ";
                    WriteExpression(*argument);
                }
                _stream << ")";
                break;
            }
            default:
                break;
        }
    }

    std::ostringstream _stream;
    int _indent = 0;
};

}  // namespace

std::string DumpBoundProgram(const BoundProgram& program) {
    Dumper dumper;
    return dumper.Dump(program);
}

}  // namespace flecha::lowering
