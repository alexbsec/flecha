#ifndef FLECHA_SEMANTIC_SYMBOL_HPP
#define FLECHA_SEMANTIC_SYMBOL_HPP

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "semantic_type.hpp"
#include "source/source_range.hpp"

namespace flecha::semantic {

enum class SymbolKind {
    Function,
    Parameter,
    LocalVariable,
    ErrorType,
    StructureType
};

enum class BuiltInFunction { None, Stdout, StdoutLine, New, Free, Take };

class Symbol {
   public:
    Symbol(SymbolKind kind, std::string name, SemanticType type,
           source::SourceRange declarationRange, bool writable)
        : _kind(kind),
          _name(std::move(name)),
          _type(type),
          _declarationRange(declarationRange),
          _writable(writable) {}
    virtual ~Symbol() = default;

    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    Symbol(Symbol&&) = default;
    Symbol& operator=(Symbol&&) = default;

    [[nodiscard]] SymbolKind Kind() const { return _kind; }
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] SemanticType Type() const { return _type; }
    [[nodiscard]] const source::SourceRange& DeclarationRange() const {
        return _declarationRange;
    }
    [[nodiscard]] bool Writable() const { return _writable; }

   private:
    SymbolKind _kind;
    std::string _name;
    SemanticType _type;
    source::SourceRange _declarationRange;
    bool _writable;
};

class VariableSymbol final : public Symbol {
   public:
    VariableSymbol(SymbolKind kind, std::string name, SemanticType type,
                   source::SourceRange declarationRange, bool writable)
        : Symbol(kind, std::move(name), type, declarationRange, writable) {}
};

class FunctionSymbol final : public Symbol {
   public:
    FunctionSymbol(std::string name, SemanticType returnType,
                   std::vector<SemanticType> parameterTypes,
                   source::SourceRange declarationRange,
                   BuiltInFunction builtIn = BuiltInFunction::None,
                   bool isUnsafe = false, const StructureType* owner = nullptr,
                   bool isPrivate = false, bool hasReceiver = false)
        : Symbol(SymbolKind::Function, std::move(name),
                 SemanticType(SemanticTypeKind::Function), declarationRange,
                 false),
          _returnType(returnType),
          _parameterTypes(std::move(parameterTypes)),
          _builtIn(builtIn),
          _isUnsafe(isUnsafe),
          _owner(owner),
          _isPrivate(isPrivate),
          _hasReceiver(hasReceiver) {}

    [[nodiscard]] SemanticType ReturnType() const { return _returnType; }
    [[nodiscard]] const std::vector<SemanticType>& ParameterTypes() const {
        return _parameterTypes;
    }
    [[nodiscard]] bool IsUnsafe() const { return _isUnsafe; }
    const StructureType* Owner() const { return _owner; }
    bool IsPrivate() const { return _isPrivate; }
    bool HasReceiver() const { return _hasReceiver; }
    [[nodiscard]] BuiltInFunction BuiltIn() const { return _builtIn; }

   private:
    SemanticType _returnType;
    std::vector<SemanticType> _parameterTypes;
    BuiltInFunction _builtIn;
    bool _isUnsafe;
    const StructureType* _owner;
    bool _isPrivate;
    bool _hasReceiver;
};

struct StructureField final {
    std::string name;
    SemanticType type;
    const core::StructureField* syntax;
};

struct StructureType final {
    std::string name;
    const core::StructureDeclaration* syntax = nullptr;
    std::vector<StructureField> fields;
    std::unordered_map<std::string, const FunctionSymbol*> methods;
    const FunctionSymbol* init = nullptr;
    const FunctionSymbol* tear = nullptr;
    bool validLayout = false;
};

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SYMBOL_HPP
