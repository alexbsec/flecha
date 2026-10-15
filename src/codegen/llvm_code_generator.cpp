#include "codegen/llvm_code_generator.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace flecha::codegen {
namespace {

bool IsUnsigned(semantic::SemanticType type) {
    switch (type.Kind()) {
        case semantic::SemanticTypeKind::U8:
        case semantic::SemanticTypeKind::U16:
        case semantic::SemanticTypeKind::U32:
        case semantic::SemanticTypeKind::U64:
        case semantic::SemanticTypeKind::USize:
            return true;
        default:
            return false;
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
        case semantic::SemanticTypeKind::StringView:
            return StringViewType();
        default:
            Fail("LLVM type is not implemented for '" +
                 std::string(type.Name()) + "'");
            return nullptr;
    }
}

llvm::StructType* LlvmCodeGenerator::StringViewType() {
    const unsigned width = _dataLayout.has_value()
                               ? _dataLayout->getPointerSizeInBits()
                               : 64;
    return llvm::StructType::get(
        _context, {llvm::PointerType::getUnqual(_context),
                   llvm::IntegerType::get(_context, width)});
}

llvm::Value* LlvmCodeGenerator::EmitRuntimeOutput(
    llvm::Value* value, semantic::SemanticType type, bool newline) {
    std::string suffix;
    std::vector<llvm::Value*> arguments;
    std::vector<llvm::Type*> parameterTypes;
    if (type.Kind() == semantic::SemanticTypeKind::StringView) {
        suffix = "string_view";
        llvm::Value* data =
            _builder.CreateExtractValue(value, 0, "stdout.data");
        llvm::Value* length =
            _builder.CreateExtractValue(value, 1, "stdout.length");
        arguments = {data, length};
        parameterTypes = {data->getType(), length->getType()};
    } else {
        switch (type.Kind()) {
            case semantic::SemanticTypeKind::I8:
                suffix = "i8";
                break;
            case semantic::SemanticTypeKind::I16:
                suffix = "i16";
                break;
            case semantic::SemanticTypeKind::I32:
                suffix = "i32";
                break;
            case semantic::SemanticTypeKind::I64:
                suffix = "i64";
                break;
            case semantic::SemanticTypeKind::ISize:
                suffix = "isize";
                break;
            case semantic::SemanticTypeKind::U8:
                suffix = "u8";
                break;
            case semantic::SemanticTypeKind::U16:
                suffix = "u16";
                break;
            case semantic::SemanticTypeKind::U32:
                suffix = "u32";
                break;
            case semantic::SemanticTypeKind::U64:
                suffix = "u64";
                break;
            case semantic::SemanticTypeKind::USize:
                suffix = "usize";
                break;
            case semantic::SemanticTypeKind::F32:
                suffix = "f32";
                break;
            case semantic::SemanticTypeKind::F64:
                suffix = "f64";
                break;
            case semantic::SemanticTypeKind::Bool:
                suffix = "bool";
                value = _builder.CreateZExt(
                    value, llvm::Type::getInt8Ty(_context), "stdout.bool");
                break;
            case semantic::SemanticTypeKind::Char:
                suffix = "char";
                break;
            default:
                Fail("stdout does not support type '" +
                     std::string(type.Name()) + "'");
                return nullptr;
        }
        arguments = {value};
        parameterTypes = {value->getType()};
    }

    llvm::FunctionType* outputType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(_context), parameterTypes, false);
    llvm::FunctionCallee output = _module->getOrInsertFunction(
        "flecha_runtime_v1_stdout_" + suffix, outputType);
    llvm::Value* call = _builder.CreateCall(output, arguments);
    if (!newline) return call;

    llvm::FunctionType* newlineType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(_context), {}, false);
    llvm::FunctionCallee emitNewline = _module->getOrInsertFunction(
        "flecha_runtime_v1_stdout_newline", newlineType);
    return _builder.CreateCall(emitNewline);
}

llvm::Value* LlvmCodeGenerator::EmitStringViewOperation(
    const lowering::BoundStringViewOperationExpression& expression) {
    llvm::Value* receiver = EmitExpression(expression.Receiver());
    if (receiver == nullptr) return nullptr;
    llvm::Value* data =
        _builder.CreateExtractValue(receiver, 0, "stringv.data");
    llvm::Value* length =
        _builder.CreateExtractValue(receiver, 1, "stringv.length");

    if (expression.Operation() == lowering::StringViewOperation::Length) {
        return _builder.CreateZExtOrTrunc(
            length, llvm::Type::getInt32Ty(_context), "stringv.len");
    }

    std::vector<llvm::Value*> arguments;
    for (const std::unique_ptr<lowering::BoundExpression>& argument :
         expression.Arguments()) {
        llvm::Value* value = EmitExpression(*argument);
        if (value == nullptr) return nullptr;
        arguments.push_back(_builder.CreateZExtOrTrunc(
            value, llvm::Type::getInt32Ty(_context), "stringv.index"));
    }

    if (expression.Operation() == lowering::StringViewOperation::At) {
        llvm::FunctionType* atType = llvm::FunctionType::get(
            llvm::Type::getInt8Ty(_context),
            {data->getType(), length->getType(),
             llvm::Type::getInt32Ty(_context)},
            false);
        llvm::FunctionCallee at = _module->getOrInsertFunction(
            "flecha_runtime_v1_string_view_at", atType);
        return _builder.CreateCall(at, {data, length, arguments.front()},
                                   "stringv.at");
    }

    llvm::FunctionType* checkType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(_context),
        {length->getType(), llvm::Type::getInt32Ty(_context),
         llvm::Type::getInt32Ty(_context)},
        false);
    llvm::FunctionCallee check = _module->getOrInsertFunction(
        "flecha_runtime_v1_string_view_check_substr", checkType);
    _builder.CreateCall(check, {length, arguments[0], arguments[1]});
    llvm::Value* start = _builder.CreateZExt(arguments[0], length->getType(),
                                             "stringv.substr.start");
    llvm::Value* substringData =
        _builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(_context), data,
                                   start, "stringv.substr.data");
    llvm::Value* substringLength = _builder.CreateZExt(
        arguments[1], length->getType(), "stringv.substr.length");
    llvm::StructType* type = StringViewType();
    llvm::Value* substring = llvm::UndefValue::get(type);
    substring = _builder.CreateInsertValue(substring, substringData, 0);
    return _builder.CreateInsertValue(substring, substringLength, 1,
                                      "stringv.substr");
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
    _positions.clear();
    _proceedTargets.clear();
    _breakTargets.clear();
    llvm::BasicBlock* entry =
        llvm::BasicBlock::Create(_context, "entry", _currentFunction);
    _builder.SetInsertPoint(entry);
    DeclarePositions(boundFunction.Body());

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

bool LlvmCodeGenerator::EmitBlock(const lowering::BoundBlockStatement& block) {
    for (const std::unique_ptr<lowering::BoundStatement>& statement :
         block.Statements()) {
        if (_builder.GetInsertBlock()->getTerminator() != nullptr &&
            statement->Kind() != lowering::BoundNodeKind::PositionStatement) {
            llvm::BasicBlock* unreachableBlock = llvm::BasicBlock::Create(
                _context, "unreachable", _currentFunction);
            _builder.SetInsertPoint(unreachableBlock);
        }
        if (!EmitStatement(*statement)) return false;
    }
    return true;
}

void LlvmCodeGenerator::DeclarePositions(
    const lowering::BoundBlockStatement& block) {
    for (const std::unique_ptr<lowering::BoundStatement>& statement :
         block.Statements()) {
        switch (statement->Kind()) {
            case lowering::BoundNodeKind::PositionStatement: {
                const lowering::BoundPositionStatement& position =
                    static_cast<const lowering::BoundPositionStatement&>(
                        *statement);
                _positions.emplace(
                    position.Name(),
                    llvm::BasicBlock::Create(_context,
                                             "position." + position.Name(),
                                             _currentFunction));
                break;
            }
            case lowering::BoundNodeKind::IfStatement: {
                const lowering::BoundIfStatement& ifStatement =
                    static_cast<const lowering::BoundIfStatement&>(*statement);
                DeclarePositions(ifStatement.ThenBlock());
                if (ifStatement.ElseBlock() != nullptr) {
                    DeclarePositions(*ifStatement.ElseBlock());
                }
                break;
            }
            case lowering::BoundNodeKind::WhileStatement:
                DeclarePositions(
                    static_cast<const lowering::BoundWhileStatement&>(
                        *statement)
                        .Body());
                break;
            default:
                break;
        }
    }
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
            llvm::Value* value = EmitExpression(*returnStatement.Expression());
            if (value == nullptr) return false;
            _builder.CreateRet(value);
            return true;
        }
        case lowering::BoundNodeKind::IfStatement:
            return EmitIf(
                static_cast<const lowering::BoundIfStatement&>(statement));
        case lowering::BoundNodeKind::WhileStatement:
            return EmitWhile(
                static_cast<const lowering::BoundWhileStatement&>(statement));
        case lowering::BoundNodeKind::ProceedStatement:
            _builder.CreateBr(_proceedTargets.back());
            return true;
        case lowering::BoundNodeKind::BreakStatement:
            _builder.CreateBr(_breakTargets.back());
            return true;
        case lowering::BoundNodeKind::JumpToStatement: {
            const lowering::BoundJumpToStatement& jump =
                static_cast<const lowering::BoundJumpToStatement&>(statement);
            _builder.CreateBr(_positions.at(jump.Position()));
            return true;
        }
        case lowering::BoundNodeKind::PositionStatement:
            return EmitPosition(
                static_cast<const lowering::BoundPositionStatement&>(
                    statement));
        default:
            return Fail("unsupported bound statement");
    }
}

bool LlvmCodeGenerator::EmitIf(const lowering::BoundIfStatement& statement) {
    llvm::Value* condition = EmitExpression(statement.Condition());
    if (condition == nullptr) return false;

    llvm::BasicBlock* thenBlock =
        llvm::BasicBlock::Create(_context, "if.then", _currentFunction);
    llvm::BasicBlock* elseBlock =
        statement.ElseBlock() == nullptr
            ? nullptr
            : llvm::BasicBlock::Create(_context, "if.else", _currentFunction);
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

bool LlvmCodeGenerator::EmitWhile(
    const lowering::BoundWhileStatement& statement) {
    llvm::BasicBlock* conditionBlock =
        llvm::BasicBlock::Create(_context, "while.condition", _currentFunction);
    llvm::BasicBlock* bodyBlock =
        llvm::BasicBlock::Create(_context, "while.body", _currentFunction);
    llvm::BasicBlock* endBlock =
        llvm::BasicBlock::Create(_context, "while.end", _currentFunction);
    _builder.CreateBr(conditionBlock);

    _builder.SetInsertPoint(conditionBlock);
    llvm::Value* condition = EmitExpression(statement.Condition());
    if (condition == nullptr) return false;
    _builder.CreateCondBr(condition, bodyBlock, endBlock);

    _builder.SetInsertPoint(bodyBlock);
    _proceedTargets.push_back(conditionBlock);
    _breakTargets.push_back(endBlock);
    if (!EmitBlock(statement.Body())) return false;
    _proceedTargets.pop_back();
    _breakTargets.pop_back();
    if (_builder.GetInsertBlock()->getTerminator() == nullptr) {
        _builder.CreateBr(conditionBlock);
    }

    _builder.SetInsertPoint(endBlock);
    return true;
}

bool LlvmCodeGenerator::EmitPosition(
    const lowering::BoundPositionStatement& statement) {
    llvm::BasicBlock* target = _positions.at(statement.Name());
    llvm::BasicBlock* current = _builder.GetInsertBlock();
    if (current != target && current->getTerminator() == nullptr) {
        _builder.CreateBr(target);
    }
    _builder.SetInsertPoint(target);
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
        const unsigned char value =
            expression.Value().empty()
                ? 0
                : static_cast<unsigned char>(expression.Value().front());
        return llvm::ConstantInt::get(type, value);
    }
    if (expression.Type().Kind() == semantic::SemanticTypeKind::StringView) {
        llvm::GlobalVariable* global =
            _builder.CreateGlobalString(expression.Value());
        llvm::Value* zero =
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(_context), 0);
        llvm::Value* data = _builder.CreateInBoundsGEP(
            global->getValueType(), global, {zero, zero}, "stringv.data");
        llvm::StructType* type = StringViewType();
        llvm::Value* value = llvm::UndefValue::get(type);
        value = _builder.CreateInsertValue(value, data, 0, "stringv.data");
        llvm::Value* length = llvm::ConstantInt::get(
            type->getElementType(1), expression.Value().size());
        return _builder.CreateInsertValue(value, length, 1, "stringv");
    }
    if (expression.Type().IsInteger()) {
        return llvm::ConstantInt::get(
            type,
            llvm::APInt(type->getIntegerBitWidth(), expression.Value(), 10));
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
        case TokenType::Add:
            return operand;
        case TokenType::Sub:
            return expression.Type().IsFloating()
                       ? _builder.CreateFNeg(operand, "fneg")
                       : _builder.CreateNeg(operand, "neg");
        case TokenType::Bang:
            return _builder.CreateNot(operand, "not");
        default:
            Fail("unsupported LLVM unary operator");
            return nullptr;
    }
}

llvm::Value* LlvmCodeGenerator::EmitCast(
    const lowering::BoundCastExpression& expression) {
    llvm::Value* value = EmitExpression(expression.Value());
    if (value == nullptr) return nullptr;
    llvm::Type* targetType = MapType(expression.Type());
    if (targetType == nullptr) return nullptr;
    const semantic::SemanticType sourceType = expression.Value().Type();
    if (sourceType == expression.Type()) return value;

    if (sourceType.IsInteger() && expression.Type().IsInteger()) {
        return _builder.CreateIntCast(value, targetType,
                                      !IsUnsigned(sourceType), "intcast");
    }
    if (sourceType.IsInteger() && expression.Type().IsFloating()) {
        return IsUnsigned(sourceType)
                   ? _builder.CreateUIToFP(value, targetType, "uitofp")
                   : _builder.CreateSIToFP(value, targetType, "sitofp");
    }
    if (sourceType.IsFloating() && expression.Type().IsInteger()) {
        return IsUnsigned(expression.Type())
                   ? _builder.CreateFPToUI(value, targetType, "fptoui")
                   : _builder.CreateFPToSI(value, targetType, "fptosi");
    }
    if (sourceType.IsFloating() && expression.Type().IsFloating()) {
        return _builder.CreateFPCast(value, targetType, "fpcast");
    }
    Fail("unsupported LLVM cast");
    return nullptr;
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
    llvm::PHINode* result =
        _builder.CreatePHI(llvm::Type::getInt1Ty(_context), 2, "logic");
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
    if (expression.Left().Type().Kind() ==
            semantic::SemanticTypeKind::StringView &&
        expression.Right().Type().Kind() ==
            semantic::SemanticTypeKind::StringView &&
        (expression.OperatorType() == TokenType::Equal ||
         expression.OperatorType() == TokenType::NotEqual ||
         expression.OperatorType() == TokenType::StrictEqual ||
         expression.OperatorType() == TokenType::StrictNotEqual)) {
        const bool negate = expression.OperatorType() == TokenType::NotEqual ||
                            expression.OperatorType() ==
                                TokenType::StrictNotEqual;
        return EmitStringViewEquality(left, right, negate);
    }
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
        case TokenType::And:
            return _builder.CreateAnd(left, right, "and");
        case TokenType::Or:
            return _builder.CreateOr(left, right, "or");
        case TokenType::Xor:
            return _builder.CreateXor(left, right, "xor");
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

llvm::Value* LlvmCodeGenerator::EmitStringViewEquality(
    llvm::Value* left, llvm::Value* right, bool negate) {
    llvm::Value* leftData = _builder.CreateExtractValue(left, 0, "left.data");
    llvm::Value* leftLength =
        _builder.CreateExtractValue(left, 1, "left.length");
    llvm::Value* rightData =
        _builder.CreateExtractValue(right, 0, "right.data");
    llvm::Value* rightLength =
        _builder.CreateExtractValue(right, 1, "right.length");
    llvm::Value* sameLength =
        _builder.CreateICmpEQ(leftLength, rightLength, "same.length");

    llvm::BasicBlock* lengthBlock = _builder.GetInsertBlock();
    llvm::BasicBlock* contentBlock =
        llvm::BasicBlock::Create(_context, "stringv.content", _currentFunction);
    llvm::BasicBlock* mergeBlock =
        llvm::BasicBlock::Create(_context, "stringv.end", _currentFunction);
    _builder.CreateCondBr(sameLength, contentBlock, mergeBlock);

    _builder.SetInsertPoint(contentBlock);
    llvm::Type* lengthType = leftLength->getType();
    llvm::FunctionType* compareType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(_context),
        {llvm::PointerType::getUnqual(_context),
         llvm::PointerType::getUnqual(_context), lengthType},
        false);
    llvm::FunctionCallee compare =
        _module->getOrInsertFunction("memcmp", compareType);
    llvm::Value* comparison =
        _builder.CreateCall(compare, {leftData, rightData, leftLength},
                            "stringv.compare");
    llvm::Value* sameContent = _builder.CreateICmpEQ(
        comparison, llvm::ConstantInt::get(llvm::Type::getInt32Ty(_context), 0),
        "same.content");
    _builder.CreateBr(mergeBlock);
    llvm::BasicBlock* completedContentBlock = _builder.GetInsertBlock();

    _builder.SetInsertPoint(mergeBlock);
    llvm::PHINode* equal =
        _builder.CreatePHI(llvm::Type::getInt1Ty(_context), 2, "stringv.equal");
    equal->addIncoming(llvm::ConstantInt::getFalse(_context), lengthBlock);
    equal->addIncoming(sameContent, completedContentBlock);
    return negate ? _builder.CreateNot(equal, "stringv.not_equal") : equal;
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
        case lowering::BoundNodeKind::CastExpression:
            return EmitCast(
                static_cast<const lowering::BoundCastExpression&>(expression));
        case lowering::BoundNodeKind::BinaryExpression:
            return EmitBinary(
                static_cast<const lowering::BoundBinaryExpression&>(
                    expression));
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
            if (call.Function().BuiltIn() ==
                semantic::BuiltInFunction::Stdout) {
                return EmitRuntimeOutput(
                    arguments.front(), call.Arguments().front()->Type(), false);
            }
            if (call.Function().BuiltIn() ==
                semantic::BuiltInFunction::StdoutLine) {
                return EmitRuntimeOutput(
                    arguments.front(), call.Arguments().front()->Type(), true);
            }
            llvm::Function* function = _functions.at(&call.Function());
            return _builder.CreateCall(
                function, arguments,
                function->getReturnType()->isVoidTy() ? "" : "call");
        }
        case lowering::BoundNodeKind::StringViewOperationExpression:
            return EmitStringViewOperation(
                static_cast<
                    const lowering::BoundStringViewOperationExpression&>(
                    expression));
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
