#ifndef FLECHA_SEMANTIC_SYMBOL_HPP
#define FLECHA_SEMANTIC_SYMBOL_HPP

#include <string>
#include <utility>
#include <vector>

#include "semantic_type.hpp"
#include "source/source_range.hpp"

namespace flecha::semantic {

enum class SymbolKind { Function, Parameter, LocalVariable };

enum class BuiltInFunction { None, Stdout, StdoutLine };

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
                   BuiltInFunction builtIn = BuiltInFunction::None)
        : Symbol(SymbolKind::Function, std::move(name),
                 SemanticType(SemanticTypeKind::Function), declarationRange,
                 false),
          _returnType(returnType),
          _parameterTypes(std::move(parameterTypes)),
          _builtIn(builtIn) {}

    [[nodiscard]] SemanticType ReturnType() const { return _returnType; }
    [[nodiscard]] const std::vector<SemanticType>& ParameterTypes() const {
        return _parameterTypes;
    }
    [[nodiscard]] BuiltInFunction BuiltIn() const { return _builtIn; }

   private:
    SemanticType _returnType;
    std::vector<SemanticType> _parameterTypes;
    BuiltInFunction _builtIn;
};

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SYMBOL_HPP
