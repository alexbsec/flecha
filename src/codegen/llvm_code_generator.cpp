#include "codegen/llvm_code_generator.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

namespace flecha::codegen {
namespace {

bool IsUnsigned(semantic::SemanticType type) {
    switch (type.Kind()) {
        case semantic::SemanticTypeKind::U8:
        case semantic::SemanticTypeKind::U16:
        case semantic::SemanticTypeKind::U32:
        case semantic::SemanticTypeKind::U64:
        case semantic::SemanticTypeKind::USize: return true;
        default: return false;
    }
}

}  // namespace

LlvmCodeGenerator::LlvmCodeGenerator() : _builder(_context) {}

void LlvmCodeGenerator::ConfigureTarget(std::string targetTriple,
                                        llvm::DataLayout dataLayout) {
    _targetTriple = std::move(targetTriple);
    _dataLayout = std::move(dataLayout);
}

bool LlvmCodeGenerator::Fail(std::string message) {
    if (_error.empty()) _error = std::move(message);
    return false;
}

llvm::Type* LlvmCodeGenerator::MapType(semantic::SemanticType type) {
    switch (type.Kind()) {
        case semantic::SemanticTypeKind::I8:
        case semantic::SemanticTypeKind::U8:
        case semantic::SemanticTypeKind::Char:
            return llvm::Type::getInt8Ty(_context);
        case semantic::SemanticTypeKind::I16:
        case semantic::SemanticTypeKind::U16:
            return llvm::Type::getInt16Ty(_context);
        case semantic::SemanticTypeKind::I32:
        case semantic::SemanticTypeKind::U32:
            return llvm::Type::getInt32Ty(_context);
        case semantic::SemanticTypeKind::I64:
        case semantic::SemanticTypeKind::U64:
            return llvm::Type::getInt64Ty(_context);
        case semantic::SemanticTypeKind::ISize:
        case semantic::SemanticTypeKind::USize: {
            const unsigned width = _dataLayout.has_value()
                                       ? _dataLayout->getPointerSizeInBits()
                                       : 64;
            return llvm::IntegerType::get(_context, width);
        }
        case semantic::SemanticTypeKind::F32:
            return llvm::Type::getFloatTy(_context);
        case semantic::SemanticTypeKind::F64:
            return llvm::Type::getDoubleTy(_context);
        case semantic::SemanticTypeKind::Bool:
            return llvm::Type::getInt1Ty(_context);
        case semantic::SemanticTypeKind::Void:
            return llvm::Type::getVoidTy(_context);
        default:
            Fail("LLVM type is not implemented for '" +
                 std::string(type.Name()) + "'");
            return nullptr;
    }
}

llvm::FunctionType* LlvmCodeGenerator::MapFunctionType(
    const semantic::FunctionSymbol& function) {
    llvm::Type* returnType = MapType(function.ReturnType());
    if (returnType == nullptr) return nullptr;
    std::vector<llvm::Type*> parameters;
    parameters.reserve(function.ParameterTypes().size());
    for (semantic::SemanticType parameter : function.ParameterTypes()) {
        llvm::Type* parameterType = MapType(parameter);
        if (parameterType == nullptr) return nullptr;
        parameters.push_back(parameterType);
    }
    return llvm::FunctionType::get(returnType, parameters, false);
}

bool LlvmCodeGenerator::DeclareFunctions(
    const lowering::BoundProgram& program) {
    for (const std::unique_ptr<lowering::BoundFunction>& boundFunction :
         program.Functions()) {
        const semantic::FunctionSymbol& symbol = boundFunction->Symbol();
        llvm::FunctionType* type = MapFunctionType(symbol);
        if (type == nullptr) return false;
        llvm::Function* function = llvm::Function::Create(
            type, llvm::Function::ExternalLinkage, symbol.Name(), *_module);
        _functions.emplace(&symbol, function);
    }
    return true;
}

llvm::AllocaInst* LlvmCodeGenerator::CreateEntryAlloca(
    const semantic::VariableSymbol& variable) {
    llvm::Type* type = MapType(variable.Type());
    if (type == nullptr) return nullptr;
    llvm::IRBuilder<> entryBuilder(_context);
    llvm::BasicBlock& entry = _currentFunction->getEntryBlock();
    entryBuilder.SetInsertPoint(&entry, entry.begin());
    return entryBuilder.CreateAlloca(type, nullptr, variable.Name());
}

bool LlvmCodeGenerator::EmitFunction(
    const lowering::BoundFunction& boundFunction) {
    _currentFunction = _functions.at(&boundFunction.Symbol());
    _variables.clear();
    llvm::BasicBlock* entry =
        llvm::BasicBlock::Create(_context, "entry", _currentFunction);
    _builder.SetInsertPoint(entry);

    std::size_t index = 0;
    for (llvm::Argument& argument : _currentFunction->args()) {
        const semantic::VariableSymbol& parameter =
            *boundFunction.Parameters()[index++];
        argument.setName(parameter.Name());
        llvm::AllocaInst* storage = CreateEntryAlloca(parameter);
        if (storage == nullptr) return false;
        _builder.CreateStore(&argument, storage);
        _variables.emplace(&parameter, storage);
    }

    if (!EmitBlock(boundFunction.Body())) return false;
    if (_builder.GetInsertBlock()->getTerminator() == nullptr) {
        if (boundFunction.Symbol().ReturnType().Kind() ==
            semantic::SemanticTypeKind::Void) {
            _builder.CreateRetVoid();
        } else {
            _builder.CreateUnreachable();
        }
    }
    _currentFunction = nullptr;
    return true;
}

bool LlvmCodeGenerator::EmitBlock(
    const lowering::BoundBlockStatement& block) {
    for (const std::unique_ptr<lowering::BoundStatement>& statement :
         block.Statements()) {
        if (_builder.GetInsertBlock()->getTerminator() != nullptr) break;
        if (!EmitStatement(*statement)) return false;
    }
    return true;
}

bool LlvmCodeGenerator::EmitStatement(
    const lowering::BoundStatement& statement) {
    switch (statement.Kind()) {
        case lowering::BoundNodeKind::VariableDeclarationStatement: {
            const lowering::BoundVariableDeclarationStatement& declaration =
                static_cast<const lowering::BoundVariableDeclarationStatement&>(
                    statement);
            llvm::Value* value = EmitExpression(declaration.Initializer());
            if (value == nullptr) return false;
            llvm::AllocaInst* storage =
                CreateEntryAlloca(declaration.Variable());
            if (storage == nullptr) return false;
            _builder.CreateStore(value, storage);
            _variables.emplace(&declaration.Variable(), storage);
            return true;
        }
        case lowering::BoundNodeKind::ExpressionStatement:
            return EmitExpression(
                       static_cast<const lowering::BoundExpressionStatement&>(
                           statement)
                           .Expression()) != nullptr;
        case lowering::BoundNodeKind::ReturnStatement: {
            const lowering::BoundReturnStatement& returnStatement =
                static_cast<const lowering::BoundReturnStatement&>(statement);
            if (returnStatement.Expression() == nullptr) {
                _builder.CreateRetVoid();
                return true;
            }
            llvm::Value* value =
                EmitExpression(*returnStatement.Expression());
            if (value == nullptr) return false;
            _builder.CreateRet(value);
            return true;
        }
        case lowering::BoundNodeKind::IfStatement:
            return EmitIf(
                static_cast<const lowering::BoundIfStatement&>(statement));
        default: return Fail("unsupported bound statement");
    }
}

bool LlvmCodeGenerator::EmitIf(
    const lowering::BoundIfStatement& statement) {
    llvm::Value* condition = EmitExpression(statement.Condition());
    if (condition == nullptr) return false;

    llvm::BasicBlock* thenBlock =
        llvm::BasicBlock::Create(_context, "if.then", _currentFunction);
    llvm::BasicBlock* elseBlock = statement.ElseBlock() == nullptr
                                      ? nullptr
                                      : llvm::BasicBlock::Create(
                                            _context, "if.else",
                                            _currentFunction);
    llvm::BasicBlock* mergeBlock =
        llvm::BasicBlock::Create(_context, "if.end", _currentFunction);
    _builder.CreateCondBr(condition, thenBlock,
                          elseBlock == nullptr ? mergeBlock : elseBlock);

    _builder.SetInsertPoint(thenBlock);
    if (!EmitBlock(statement.ThenBlock())) return false;
    if (_builder.GetInsertBlock()->getTerminator() == nullptr) {
        _builder.CreateBr(mergeBlock);
    }

    if (elseBlock != nullptr) {
        _builder.SetInsertPoint(elseBlock);
        if (!EmitBlock(*statement.ElseBlock())) return false;
        if (_builder.GetInsertBlock()->getTerminator() == nullptr) {
            _builder.CreateBr(mergeBlock);
        }
    }
    _builder.SetInsertPoint(mergeBlock);
    return true;
}

llvm::Value* LlvmCodeGenerator::EmitLiteral(
    const lowering::BoundLiteralExpression& expression) {
    llvm::Type* type = MapType(expression.Type());
    if (type == nullptr) return nullptr;
    if (expression.Type().Kind() == semantic::SemanticTypeKind::Bool) {
        return llvm::ConstantInt::get(type, expression.Value() == "true");
    }
    if (expression.Type().Kind() == semantic::SemanticTypeKind::Char) {
        const unsigned char value = expression.Value().empty()
                                        ? 0
                                        : static_cast<unsigned char>(
                                              expression.Value().front());
        return llvm::ConstantInt::get(type, value);
    }
    if (expression.Type().IsInteger()) {
        return llvm::ConstantInt::get(
            type, llvm::APInt(type->getIntegerBitWidth(), expression.Value(),
                              10));
    }
    if (expression.Type().IsFloating()) {
        char* end = nullptr;
        const double value = std::strtod(expression.Value().c_str(), &end);
        if (end == expression.Value().c_str() || *end != '\0') {
            Fail("invalid floating-point literal");
            return nullptr;
        }
        return llvm::ConstantFP::get(type, value);
    }
    Fail("unsupported LLVM literal type");
    return nullptr;
}

llvm::Value* LlvmCodeGenerator::EmitUnary(
    const lowering::BoundUnaryExpression& expression) {
    llvm::Value* operand = EmitExpression(expression.Operand());
    if (operand == nullptr) return nullptr;
    switch (expression.OperatorType()) {
        case TokenType::Add: return operand;
        case TokenType::Sub:
            return expression.Type().IsFloating()
                       ? _builder.CreateFNeg(operand, "fneg")
                       : _builder.CreateNeg(operand, "neg");
        case TokenType::Bang: return _builder.CreateNot(operand, "not");
        default:
            Fail("unsupported LLVM unary operator");
            return nullptr;
    }
}

llvm::Value* LlvmCodeGenerator::EmitBooleanLogical(
    const lowering::BoundBinaryExpression& expression) {
    llvm::Value* left = EmitExpression(expression.Left());
    if (left == nullptr) return nullptr;
    llvm::BasicBlock* leftBlock = _builder.GetInsertBlock();
    llvm::BasicBlock* rightBlock =
        llvm::BasicBlock::Create(_context, "logic.right", _currentFunction);
    llvm::BasicBlock* mergeBlock =
        llvm::BasicBlock::Create(_context, "logic.end", _currentFunction);
    const bool isAnd = expression.OperatorType() == TokenType::And;
    _builder.CreateCondBr(left, isAnd ? rightBlock : mergeBlock,
                          isAnd ? mergeBlock : rightBlock);
    _builder.SetInsertPoint(rightBlock);
    llvm::Value* right = EmitExpression(expression.Right());
    if (right == nullptr) return nullptr;
    llvm::BasicBlock* completedRightBlock = _builder.GetInsertBlock();
    _builder.CreateBr(mergeBlock);
    _builder.SetInsertPoint(mergeBlock);
    llvm::PHINode* result = _builder.CreatePHI(
        llvm::Type::getInt1Ty(_context), 2, "logic");
    result->addIncoming(llvm::ConstantInt::getBool(_context, !isAnd),
                        leftBlock);
    result->addIncoming(right, completedRightBlock);
    return result;
}

llvm::Value* LlvmCodeGenerator::EmitBinary(
    const lowering::BoundBinaryExpression& expression) {
    if (expression.Type().Kind() == semantic::SemanticTypeKind::Bool &&
        (expression.OperatorType() == TokenType::And ||
         expression.OperatorType() == TokenType::Or)) {
        return EmitBooleanLogical(expression);
    }
    llvm::Value* left = EmitExpression(expression.Left());
    llvm::Value* right = EmitExpression(expression.Right());
    if (left == nullptr || right == nullptr) return nullptr;
    const bool floating = expression.Left().Type().IsFloating();
    const bool unsignedValue = IsUnsigned(expression.Left().Type());
    switch (expression.OperatorType()) {
        case TokenType::Add:
            return floating ? _builder.CreateFAdd(left, right, "fadd")
                            : _builder.CreateAdd(left, right, "add");
        case TokenType::Sub:
            return floating ? _builder.CreateFSub(left, right, "fsub")
                            : _builder.CreateSub(left, right, "sub");
        case TokenType::Mul:
            return floating ? _builder.CreateFMul(left, right, "fmul")
                            : _builder.CreateMul(left, right, "mul");
        case TokenType::Div:
            if (floating) return _builder.CreateFDiv(left, right, "fdiv");
            return unsignedValue ? _builder.CreateUDiv(left, right, "udiv")
                                 : _builder.CreateSDiv(left, right, "sdiv");
        case TokenType::Mod:
            return unsignedValue ? _builder.CreateURem(left, right, "urem")
                                 : _builder.CreateSRem(left, right, "srem");
        case TokenType::And: return _builder.CreateAnd(left, right, "and");
        case TokenType::Or: return _builder.CreateOr(left, right, "or");
        case TokenType::Xor: return _builder.CreateXor(left, right, "xor");
        case TokenType::Equal:
        case TokenType::StrictEqual:
            return floating ? _builder.CreateFCmpOEQ(left, right, "feq")
                            : _builder.CreateICmpEQ(left, right, "eq");
        case TokenType::NotEqual:
        case TokenType::StrictNotEqual:
            return floating ? _builder.CreateFCmpONE(left, right, "fne")
                            : _builder.CreateICmpNE(left, right, "ne");
        case TokenType::Less:
            if (floating) return _builder.CreateFCmpOLT(left, right, "flt");
            return unsignedValue ? _builder.CreateICmpULT(left, right, "ult")
                                 : _builder.CreateICmpSLT(left, right, "slt");
        case TokenType::LessEqual:
            if (floating) return _builder.CreateFCmpOLE(left, right, "fle");
            return unsignedValue ? _builder.CreateICmpULE(left, right, "ule")
                                 : _builder.CreateICmpSLE(left, right, "sle");
        case TokenType::Greater:
            if (floating) return _builder.CreateFCmpOGT(left, right, "fgt");
            return unsignedValue ? _builder.CreateICmpUGT(left, right, "ugt")
                                 : _builder.CreateICmpSGT(left, right, "sgt");
        case TokenType::GreaterEqual:
            if (floating) return _builder.CreateFCmpOGE(left, right, "fge");
            return unsignedValue ? _builder.CreateICmpUGE(left, right, "uge")
                                 : _builder.CreateICmpSGE(left, right, "sge");
        default:
            Fail("unsupported LLVM binary operator");
            return nullptr;
    }
}

llvm::Value* LlvmCodeGenerator::EmitExpression(
    const lowering::BoundExpression& expression) {
    switch (expression.Kind()) {
        case lowering::BoundNodeKind::LiteralExpression:
            return EmitLiteral(
                static_cast<const lowering::BoundLiteralExpression&>(
                    expression));
        case lowering::BoundNodeKind::VariableExpression: {
            const semantic::VariableSymbol& symbol =
                static_cast<const lowering::BoundVariableExpression&>(
                    expression)
                    .Symbol();
            llvm::AllocaInst* storage = _variables.at(&symbol);
            return _builder.CreateLoad(storage->getAllocatedType(), storage,
                                       symbol.Name());
        }
        case lowering::BoundNodeKind::UnaryExpression:
            return EmitUnary(
                static_cast<const lowering::BoundUnaryExpression&>(expression));
        case lowering::BoundNodeKind::BinaryExpression:
            return EmitBinary(
                static_cast<const lowering::BoundBinaryExpression&>(expression));
        case lowering::BoundNodeKind::AssignmentExpression: {
            const lowering::BoundAssignmentExpression& assignment =
                static_cast<const lowering::BoundAssignmentExpression&>(
                    expression);
            llvm::Value* value = EmitExpression(assignment.Value());
            if (value == nullptr) return nullptr;
            _builder.CreateStore(value, _variables.at(&assignment.Target()));
            return value;
        }
        case lowering::BoundNodeKind::CallExpression: {
            const lowering::BoundCallExpression& call =
                static_cast<const lowering::BoundCallExpression&>(expression);
            std::vector<llvm::Value*> arguments;
            arguments.reserve(call.Arguments().size());
            for (const std::unique_ptr<lowering::BoundExpression>& argument :
                 call.Arguments()) {
                llvm::Value* value = EmitExpression(*argument);
                if (value == nullptr) return nullptr;
                arguments.push_back(value);
            }
            llvm::Function* function = _functions.at(&call.Function());
            return _builder.CreateCall(function, arguments,
                                       function->getReturnType()->isVoidTy()
                                           ? ""
                                           : "call");
        }
        case lowering::BoundNodeKind::FunctionExpression:
            return _functions.at(
                &static_cast<const lowering::BoundFunctionExpression&>(
                     expression)
                     .Symbol());
        default:
            Fail("unsupported bound expression");
            return nullptr;
    }
}

std::unique_ptr<llvm::Module> LlvmCodeGenerator::Generate(
    const lowering::BoundProgram& program, std::string moduleName) {
    _error.clear();
    _functions.clear();
    _variables.clear();
    _module = std::make_unique<llvm::Module>(moduleName, _context);
    if (!_targetTriple.empty()) {
        _module->setTargetTriple(llvm::Triple(_targetTriple));
    }
    if (_dataLayout.has_value()) _module->setDataLayout(*_dataLayout);
    if (!DeclareFunctions(program)) return nullptr;
    for (const std::unique_ptr<lowering::BoundFunction>& function :
         program.Functions()) {
        if (!EmitFunction(*function)) return nullptr;
    }
    const std::string verificationError = VerifyModule(*_module);
    if (!verificationError.empty()) {
        Fail("LLVM module verification failed: " + verificationError);
        return nullptr;
    }
    return std::move(_module);
}

std::string VerifyModule(const llvm::Module& module) {
    std::string message;
    llvm::raw_string_ostream output(message);
    if (!llvm::verifyModule(module, &output)) return {};
    output.flush();
    return message;
}

}  // namespace flecha::codegen
