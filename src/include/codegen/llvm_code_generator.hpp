#ifndef FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP
#define FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DerivedTypes.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
    struct CleanupScope final {
        const lowering::BoundBlockStatement* block;
        std::vector<llvm::AllocaInst*> ownedStrings;
    };

    struct CleanupRequest final {
        llvm::BasicBlock* block;
        std::vector<std::shared_ptr<CleanupScope>> scopes;
    };

    struct LoopTarget final {
        llvm::BasicBlock* block;
        std::size_t retainedScopeCount;
    };

    [[nodiscard]] llvm::Type* MapType(semantic::SemanticType type);
    [[nodiscard]] llvm::StructType* StringViewType();
    [[nodiscard]] llvm::StructType* StringType();
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
    [[nodiscard]] llvm::Value* EmitStringViewEquality(
        llvm::Value* left, llvm::Value* right, bool negate);
    [[nodiscard]] llvm::Value* EmitStringConcat(
        llvm::Value* left, semantic::SemanticType leftType,
        llvm::Value* right, semantic::SemanticType rightType);
    [[nodiscard]] llvm::Value* EmitRuntimeOutput(
        llvm::Value* value, semantic::SemanticType type, bool newline);
    [[nodiscard]] llvm::Value* EmitStringViewOperation(
        const lowering::BoundStringViewOperationExpression& expression);
    [[nodiscard]] llvm::Value* EmitOwnedString(
        llvm::Value* value, semantic::SemanticType sourceType,
        bool temporary = true);
    [[nodiscard]] llvm::Value* EmitStringView(llvm::Value* value,
                                               semantic::SemanticType type);
    void EmitStringDestroy(llvm::Value* value);
    void EmitStringStorageCleanup(llvm::AllocaInst* storage);
    [[nodiscard]] llvm::AllocaInst* CreateOwnedStringTemporary(
        std::string_view name);
    void EmitCleanupsTo(std::size_t retainedScopeCount);
    void CollectPositionScopePaths(
        const lowering::BoundBlockStatement& block,
        std::vector<const lowering::BoundBlockStatement*>& path);
    [[nodiscard]] llvm::Value* CoerceValue(
        llvm::Value* value, semantic::SemanticType sourceType,
        semantic::SemanticType destinationType);
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
    std::unordered_map<
        std::string, std::vector<const lowering::BoundBlockStatement*>>
        _positionScopePaths;
    std::vector<LoopTarget> _proceedTargets;
    std::vector<LoopTarget> _breakTargets;
    std::vector<std::shared_ptr<CleanupScope>> _cleanupScopes;
    std::vector<CleanupRequest> _cleanupRequests;
    llvm::Function* _currentFunction = nullptr;
    semantic::SemanticType _currentReturnType{
        semantic::SemanticTypeKind::Void};
    std::string _error;
};

[[nodiscard]] std::string VerifyModule(const llvm::Module& module);

}  // namespace flecha::codegen

#endif  // FLECHA_CODEGEN_LLVM_CODE_GENERATOR_HPP
