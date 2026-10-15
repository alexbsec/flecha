#ifndef FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP
#define FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "lowering/bound_ir.hpp"

namespace flecha::codegen {

class LlvmCodeGenerator final {
   public:
    LlvmCodeGenerator();

    void ConfigureTarget(std::string targetTriple, llvm::DataLayout dataLayout);

    [[nodiscard]] std::unique_ptr<llvm::Module> Generate(
        const lowering::BoundProgram& program, std::string moduleName);
    [[nodiscard]] const std::string& Error() const { return _error; }

   private:
    [[nodiscard]] llvm::Type* MapType(semantic::SemanticType type);
    [[nodiscard]] llvm::FunctionType* MapFunctionType(
        const semantic::FunctionSymbol& function);
    bool DeclareFunctions(const lowering::BoundProgram& program);
    bool EmitFunction(const lowering::BoundFunction& function);
    bool EmitBlock(const lowering::BoundBlockStatement& block);
    bool EmitStatement(const lowering::BoundStatement& statement);
    bool EmitIf(const lowering::BoundIfStatement& statement);
    bool EmitWhile(const lowering::BoundWhileStatement& statement);
    void DeclarePositions(const lowering::BoundBlockStatement& block);
    bool EmitPosition(const lowering::BoundPositionStatement& statement);
    [[nodiscard]] llvm::Value* EmitExpression(
        const lowering::BoundExpression& expression);
    [[nodiscard]] llvm::Value* EmitLiteral(
        const lowering::BoundLiteralExpression& expression);
    [[nodiscard]] llvm::Value* EmitUnary(
        const lowering::BoundUnaryExpression& expression);
    [[nodiscard]] llvm::Value* EmitCast(
        const lowering::BoundCastExpression& expression);
    [[nodiscard]] llvm::Value* EmitBinary(
        const lowering::BoundBinaryExpression& expression);
    [[nodiscard]] llvm::Value* EmitBooleanLogical(
        const lowering::BoundBinaryExpression& expression);
    [[nodiscard]] llvm::AllocaInst* CreateEntryAlloca(
        const semantic::VariableSymbol& variable);
    bool Fail(std::string message);

    llvm::LLVMContext _context;
    llvm::IRBuilder<> _builder;
    std::unique_ptr<llvm::Module> _module;
    std::string _targetTriple;
    std::optional<llvm::DataLayout> _dataLayout;
    std::unordered_map<const semantic::FunctionSymbol*, llvm::Function*>
        _functions;
    std::unordered_map<const semantic::VariableSymbol*, llvm::AllocaInst*>
        _variables;
    std::unordered_map<std::string, llvm::BasicBlock*> _positions;
    std::vector<llvm::BasicBlock*> _proceedTargets;
    std::vector<llvm::BasicBlock*> _breakTargets;
    llvm::Function* _currentFunction = nullptr;
    std::string _error;
};

[[nodiscard]] std::string VerifyModule(const llvm::Module& module);

}  // namespace flecha::codegen

#endif  // FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP
