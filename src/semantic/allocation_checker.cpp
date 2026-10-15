#include "semantic/allocation_checker.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <map>
#include <set>
#include <utility>

#include "semantic/control_flow_graph.hpp"

namespace flecha::semantic {
namespace {
using core::AstNodeKind;
using diagnostics::DiagnosticCode;
using Identity = const core::AstNode*;

enum class ValueKind {
    Unknown,
    Null,
    True,
    False,
    Error,
    Integer,
    Pointer,
    Interior,
    Dangling,
    Address,
    Aggregate
};
struct Value final {
    ValueKind kind = ValueKind::Unknown;
    Identity identity = nullptr;
    const Symbol* address = nullptr;
    std::uint64_t integer = 0;
    std::vector<Value> elements;
    bool operator==(const Value&) const = default;
};
struct Resource final {
    bool live = true;
    bool owned = true;
    bool operator==(const Resource&) const = default;
};
struct Deferred final {
    const core::BlockStatement* scope = nullptr;
    const core::Expression* call = nullptr;
    const FunctionSymbol* function = nullptr;
    std::vector<Value> arguments;
    bool operator==(const Deferred&) const = default;
};
struct State final {
    std::map<const Symbol*, Value> bindings;
    std::map<const core::Expression*, Value> values;
    std::map<Identity, Resource> resources;
    std::vector<Deferred> deferred;
    bool operator==(const State&) const = default;
};
struct Outcome final {
    State state;
    Value result;
};

const core::Expression& Unwrap(const core::Expression& expression) {
    if (expression.Kind() == AstNodeKind::ParenthesizedExpression)
        return Unwrap(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    return expression;
}

// Keep paths separate: merging a successful new with its failure loses the
// relationship between its pointer and error, and invents leaks at error exits.
// The finite budget fails closed instead of silently widening away obligations.
class AllocationChecker final {
   public:
    AllocationChecker(const core::CompilationUnit& unit,
                      const SemanticModel& model,
                      diagnostics::DiagnosticBag& diagnostics)
        : _model(model), _diagnostics(diagnostics) {
        for (const auto& declaration : unit.Declarations())
            if (declaration->Kind() == AstNodeKind::FunctionDeclaration)
                Register(static_cast<const core::FunctionDeclaration&>(
                    *declaration));
        for (const auto& function : model.GeneratedFunctions())
            Register(*function);
    }

    void Analyze() {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& [function, graph] : _graphs) {
                for (const FlowNode& node : graph.nodes) {
                    const FunctionSymbol* callee =
                        node.memoryExpression ? Callee(*node.memoryExpression)
                        : node.memoryDefer    ? Callee(node.memoryDefer->Call())
                                              : nullptr;
                    const auto found = _functions.find(callee);
                    if (node.allocation ||
                        (callee &&
                         callee->BuiltIn() == BuiltInFunction::Free) ||
                        (found != _functions.end() &&
                         _resourceFunctions.contains(found->second)))
                        changed |= _resourceFunctions.insert(function).second;
                }
            }
        }
        for (const core::FunctionDeclaration* function : _ordered) {
            if (!_resourceFunctions.contains(function)) continue;
            _steps = 0;
            State state;
            std::vector<Value> arguments;
            for (const auto& parameter : function->Parameters()) {
                const Symbol* symbol = _model.DeclaredSymbol(*parameter);
                Value argument;
                if (symbol &&
                    symbol->Type().Kind() == SemanticTypeKind::Pointer) {
                    argument = {ValueKind::Pointer, parameter.get()};
                    state.resources[parameter.get()] = {true, false};
                }
                arguments.push_back(argument);
            }
            (void)Run(*function, std::move(state), arguments);
        }
    }

   private:
    void Register(const core::FunctionDeclaration& function) {
        const auto* symbol =
            static_cast<const FunctionSymbol*>(_model.DeclaredSymbol(function));
        if (!symbol) return;
        _functions[symbol] = &function;
        _graphs.emplace(&function, BuildControlFlowGraph(function, _model));
        _ordered.push_back(&function);
    }

    void Report(DiagnosticCode code, const core::AstNode& at,
                const std::string& message, Identity origin = nullptr) {
        if (!_reported.emplace(code, at.Range().start.offset).second) return;
        source::SourceRange range = at.Range();
        if (at.Kind() == AstNodeKind::BlockStatement && !range.Empty()) {
            range.start = range.end;
            --range.start.offset;
            if (range.start.column > 1) --range.start.column;
        }
        _diagnostics.Report(code, range, message);
        if (origin && origin != &at)
            _diagnostics.Report(code, origin->Range(),
                                "allocation or pointer originates here",
                                diagnostics::DiagnosticSeverity::Note);
    }

    Value Get(const State& state, const core::Expression& expression) const {
        const auto found = state.values.find(&expression);
        return found == state.values.end() ? Value{} : found->second;
    }
    Value Binding(const State& state, const Symbol* symbol) const {
        const auto found = state.bindings.find(symbol);
        return found == state.bindings.end() ? Value{} : found->second;
    }
    void Put(State& state, const core::Expression& expression, Value value) {
        if (value.kind == ValueKind::Unknown)
            state.values.erase(&expression);
        else
            state.values[&expression] = std::move(value);
    }
    void Bind(State& state, const Symbol* symbol, Value value) {
        if (!symbol) return;
        if (value.kind == ValueKind::Unknown)
            state.bindings.erase(symbol);
        else
            state.bindings[symbol] = std::move(value);
    }
    bool Contains(const Value& value, Identity identity) const {
        // An interior address can observe storage but cannot discharge its
        // allocation obligation. Only the original pointer can be returned as
        // an owner or kept as a future free argument.
        if (value.identity == identity && value.kind == ValueKind::Pointer)
            return true;
        return std::any_of(
            value.elements.begin(), value.elements.end(),
            [&](const Value& child) { return Contains(child, identity); });
    }
    bool Tracks(const Value& value) const {
        if (value.identity) return true;
        return std::any_of(value.elements.begin(), value.elements.end(),
                           [&](const Value& child) { return Tracks(child); });
    }
    void CheckUse(const State& state, const Value& value,
                  const core::AstNode& at, bool dereference = false) {
        const auto found = state.resources.find(value.identity);
        if (value.kind == ValueKind::Dangling ||
            (found != state.resources.end() && !found->second.live))
            Report(DiagnosticCode::UseAfterFree, at,
                   "pointer may be used after its allocation was freed",
                   value.identity);
        if (dereference && value.kind == ValueKind::Null)
            Report(DiagnosticCode::NullDereference, at,
                   "pointer can be null on this path; check the allocation "
                   "error or pointer first");
        for (const Value& child : value.elements) CheckUse(state, child, at);
    }
    void Free(State& state, const Value& value, const core::AstNode& at) {
        if (value.kind == ValueKind::Null) return;
        const auto found = state.resources.find(value.identity);
        if (value.kind == ValueKind::Dangling ||
            (found != state.resources.end() && !found->second.live)) {
            Report(DiagnosticCode::DoubleFree, at,
                   "allocation may be freed more than once on this path",
                   value.identity);
        } else if (value.kind == ValueKind::Interior ||
                   value.kind == ValueKind::Address) {
            Report(DiagnosticCode::InvalidFree, at,
                   "free requires the original allocation pointer, not stack "
                   "or interior storage",
                   value.identity);
        } else if (value.kind == ValueKind::Pointer &&
                   found != state.resources.end()) {
            found->second.live = false;
        } else {
            Report(DiagnosticCode::UnverifiedOwnership, at,
                   "cannot verify the allocation provenance of this free");
        }
    }

    // Reusing an allocation site must not resurrect aliases of its previous
    // freed instance. Dangling values keep their original diagnostic location.
    void Retire(Value& value, Identity identity) {
        if (value.identity == identity) value.kind = ValueKind::Dangling;
        for (Value& child : value.elements) Retire(child, identity);
    }
    bool Allocate(State& state, const core::CallExpression& call) {
        const auto previous = state.resources.find(&call);
        if (previous != state.resources.end() && previous->second.live) {
            Report(DiagnosticCode::UnverifiedOwnership, call,
                   "allocation site is revisited while its previous allocation "
                   "is still live; "
                   "release it in an inner scope before repeating");
            return false;
        }
        for (auto& [symbol, value] : state.bindings) Retire(value, &call);
        for (auto& [expression, value] : state.values) Retire(value, &call);
        for (Deferred& deferred : state.deferred)
            for (Value& argument : deferred.arguments) Retire(argument, &call);
        state.resources[&call] = {};
        Value result{ValueKind::Aggregate};
        result.elements = {{ValueKind::Pointer, &call}, {ValueKind::Null}};
        Put(state, call, std::move(result));
        return true;
    }

    void CheckLost(State& state, const core::AstNode& at,
                   const Value& returned = {}) {
        for (auto& [identity, resource] : state.resources) {
            if (!resource.live || !resource.owned ||
                Contains(returned, identity))
                continue;
            bool held = false;
            for (const auto& [symbol, value] : state.bindings)
                held |= Contains(value, identity);
            for (const auto& [expression, value] : state.values)
                held |= Contains(value, identity);
            for (const Deferred& deferred : state.deferred)
                for (const Value& value : deferred.arguments)
                    held |= Contains(value, identity);
            if (!held) {
                Report(DiagnosticCode::MemoryLeak, at,
                       "allocation can be lost without free on this path",
                       identity);
                // This obligation is already diagnosed. Avoid a cascade and
                // let the worklist converge for loops with a lost allocation.
                resource.live = false;
            }
        }
    }

    const Symbol* DirectTarget(const core::Expression& expression) const {
        const core::Expression& target = Unwrap(expression);
        return target.Kind() == AstNodeKind::NameExpression
                   ? _model.SymbolFor(
                         static_cast<const core::NameExpression&>(target))
                   : nullptr;
    }
    Value Address(const State& state,
                  const core::Expression& expression) const {
        const core::Expression& target = Unwrap(expression);
        if (const Symbol* symbol = DirectTarget(target))
            return {ValueKind::Address, nullptr, symbol};
        if (target.Kind() == AstNodeKind::UnaryExpression) {
            const auto& unary =
                static_cast<const core::UnaryExpression&>(target);
            if (unary.OperatorType() == TokenType::Mul)
                return Get(state, unary.Operand());
        }
        if (target.Kind() == AstNodeKind::MemberAccessExpression) {
            const auto& field =
                static_cast<const core::MemberAccessExpression&>(target);
            Value receiver = field.Access() == TokenType::Arrow
                                 ? Get(state, field.Receiver())
                                 : Address(state, field.Receiver());
            if (receiver.identity) {
                receiver.kind = ValueKind::Interior;
                return receiver;
            }
            return {ValueKind::Address};
        }
        if (target.Kind() == AstNodeKind::IndexExpression) {
            const auto& index =
                static_cast<const core::IndexExpression&>(target);
            Value receiver = Address(state, index.Receiver());
            if (receiver.identity) receiver.kind = ValueKind::Interior;
            receiver.address = nullptr;
            return receiver;
        }
        return {};
    }
    void Assign(State& state, const core::Expression& target, Value value) {
        if (const Symbol* symbol = DirectTarget(target)) {
            Bind(state, symbol, std::move(value));
            return;
        }
        const core::Expression& unwrapped = Unwrap(target);
        if (unwrapped.Kind() == AstNodeKind::IndexExpression) {
            const auto& index =
                static_cast<const core::IndexExpression&>(unwrapped);
            if (Owns(state, Get(state, index.Receiver())))
                Report(DiagnosticCode::UnverifiedOwnership, target,
                       "cannot verify mutation of an aggregate containing "
                       "allocation handles");
        }
        const Value address = Address(state, target);
        if (address.kind == ValueKind::Address && address.address) {
            Bind(state, address.address, std::move(value));
        } else if (Owns(state, value)) {
            Report(DiagnosticCode::UnverifiedOwnership, target,
                   "cannot verify ownership stored through a field, index, or "
                   "indirect pointer; "
                   "keep the allocation in a local binding");
        }
    }

    const FunctionSymbol* Callee(const core::Expression& expression) const {
        if (expression.Kind() == AstNodeKind::CallExpression) {
            const auto& call =
                static_cast<const core::CallExpression&>(expression);
            const Symbol* symbol = DirectTarget(call.Callee());
            return symbol && symbol->Kind() == SymbolKind::Function
                       ? static_cast<const FunctionSymbol*>(symbol)
                       : nullptr;
        }
        if (expression.Kind() == AstNodeKind::MemberCallExpression)
            return _model.MethodFor(
                static_cast<const core::MemberCallExpression&>(expression));
        return nullptr;
    }
    std::vector<Value> Arguments(const State& state,
                                 const core::Expression& expression) const {
        std::vector<Value> result;
        if (expression.Kind() == AstNodeKind::CallExpression) {
            for (const auto& argument :
                 static_cast<const core::CallExpression&>(expression)
                     .Arguments())
                result.push_back(Get(state, *argument));
        } else if (expression.Kind() == AstNodeKind::MemberCallExpression) {
            const auto& call =
                static_cast<const core::MemberCallExpression&>(expression);
            const FunctionSymbol* method = _model.MethodFor(call);
            if (method && method->HasReceiver())
                result.push_back(Get(state, call.Receiver()));
            for (const auto& argument : call.Arguments())
                result.push_back(Get(state, *argument));
        }
        return result;
    }

    bool Relevant(const FunctionSymbol* function,
                  const std::vector<Value>& arguments) const {
        for (const Value& argument : arguments)
            if (Tracks(argument) || argument.address) return true;
        if (!function) return false;
        if (function->BuiltIn() == BuiltInFunction::Free) return true;
        for (const SemanticType& type : function->ParameterTypes())
            if (type.Kind() == SemanticTypeKind::Pointer) return true;
        const auto found = _functions.find(function);
        if (found == _functions.end()) return false;
        // Calls that allocate are interpreted even without pointer arguments,
        // including factories that return an allocation in a tuple.
        return _resourceFunctions.contains(found->second);
    }

    std::vector<Outcome> Invoke(const core::Expression& expression,
                                const FunctionSymbol* function,
                                const std::vector<Value>& arguments,
                                State state) {
        if (function && function->BuiltIn() == BuiltInFunction::Free) {
            if (!arguments.empty()) Free(state, arguments[0], expression);
            return {{std::move(state), {}}};
        }
        for (const Value& argument : arguments)
            CheckUse(state, argument, expression);
        if (!Relevant(function, arguments)) return {{std::move(state), {}}};
        const auto found = _functions.find(function);
        if (found == _functions.end()) {
            if (function && function->BuiltIn() != BuiltInFunction::None)
                return {{std::move(state), {}}};
            Report(DiagnosticCode::UnverifiedOwnership, expression,
                   "cannot verify ownership effects of this call");
            return {{std::move(state), {}}};
        }
        if (std::find(_active.begin(), _active.end(), found->second) !=
            _active.end()) {
            Report(DiagnosticCode::UnverifiedOwnership, expression,
                   "recursive pointer ownership requires a contract and cannot "
                   "yet be verified");
            return {};
        }
        return Run(*found->second, std::move(state), arguments);
    }

    // Consume child temporaries only after their parent has used them. Address
    // formation needs the lvalue's children, so keep those until the address is
    // evaluated; the statement boundary then drops all remaining temporaries.
    void ForgetChildren(State& state, const core::Expression& expression) {
        switch (expression.Kind()) {
            case AstNodeKind::ParenthesizedExpression:
                state.values.erase(
                    &static_cast<const core::ParenthesizedExpression&>(
                         expression)
                         .Inner());
                break;
            case AstNodeKind::CastExpression:
                state.values.erase(
                    &static_cast<const core::CastExpression&>(expression)
                         .Value());
                break;
            case AstNodeKind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                state.values.erase(&binary.Left());
                state.values.erase(&binary.Right());
                break;
            }
            case AstNodeKind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                state.values.erase(&assignment.Target());
                state.values.erase(&assignment.Value());
                break;
            }
            case AstNodeKind::CallExpression: {
                const auto& call =
                    static_cast<const core::CallExpression&>(expression);
                state.values.erase(&call.Callee());
                for (const auto& argument : call.Arguments())
                    state.values.erase(argument.get());
                break;
            }
            case AstNodeKind::MemberCallExpression: {
                const auto& call =
                    static_cast<const core::MemberCallExpression&>(expression);
                state.values.erase(&call.Receiver());
                for (const auto& argument : call.Arguments())
                    state.values.erase(argument.get());
                break;
            }
            case AstNodeKind::TupleExpression:
                for (const auto& element :
                     static_cast<const core::TupleExpression&>(expression)
                         .Elements())
                    state.values.erase(element.get());
                break;
            case AstNodeKind::ArrayExpression:
                for (const auto& element :
                     static_cast<const core::ArrayExpression&>(expression)
                         .Elements())
                    state.values.erase(element.get());
                break;
            default:
                break;
        }
    }

    Value Evaluate(State& state, const core::Expression& expression) {
        switch (expression.Kind()) {
            case AstNodeKind::LiteralExpression: {
                const auto& literal =
                    static_cast<const core::LiteralExpression&>(expression);
                switch (literal.LiteralType()) {
                    case TokenType::Null:
                    case TokenType::None:
                        return {ValueKind::Null};
                    case TokenType::True:
                        return {ValueKind::True};
                    case TokenType::False:
                        return {ValueKind::False};
                    case TokenType::NumberLiteral: {
                        Value result{ValueKind::Integer};
                        const std::string& text = literal.Value();
                        const auto parsed = std::from_chars(
                            text.data(), text.data() + text.size(),
                            result.integer);
                        return parsed.ec == std::errc{} &&
                                       parsed.ptr == text.data() + text.size()
                                   ? result
                                   : Value{};
                    }
                    default:
                        return {};
                }
            }
            case AstNodeKind::NameExpression:
                return Binding(
                    state,
                    _model.SymbolFor(
                        static_cast<const core::NameExpression&>(expression)));
            case AstNodeKind::ParenthesizedExpression:
                return Get(state,
                           static_cast<const core::ParenthesizedExpression&>(
                               expression)
                               .Inner());
            case AstNodeKind::CastExpression:
                return Get(state,
                           static_cast<const core::CastExpression&>(expression)
                               .Value());
            case AstNodeKind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                Value value = Get(state, assignment.Value());
                CheckUse(state, value, expression);
                Assign(state, assignment.Target(), value);
                return value;
            }
            case AstNodeKind::UnaryExpression: {
                const auto& unary =
                    static_cast<const core::UnaryExpression&>(expression);
                Value operand = Get(state, unary.Operand());
                if (unary.OperatorType() == TokenType::RawAddress ||
                    unary.OperatorType() == TokenType::Ampersand) {
                    const core::Expression& target = Unwrap(unary.Operand());
                    if (target.Kind() == AstNodeKind::IndexExpression &&
                        Owns(state,
                             Get(state,
                                 static_cast<const core::IndexExpression&>(
                                     target)
                                     .Receiver())))
                        Report(DiagnosticCode::UnverifiedOwnership, expression,
                               "cannot track an address into an aggregate "
                               "containing allocation handles");
                    return Address(state, unary.Operand());
                }
                if (unary.OperatorType() == TokenType::Mul) {
                    CheckUse(state, operand, expression, true);
                    if (operand.kind == ValueKind::Address)
                        return Binding(state, operand.address);
                    return {};
                }
                if (unary.OperatorType() == TokenType::Increment)
                    Assign(state, unary.Operand(), {});
                if (unary.OperatorType() == TokenType::Bang) {
                    if (operand.kind == ValueKind::True)
                        return {ValueKind::False};
                    if (operand.kind == ValueKind::False)
                        return {ValueKind::True};
                }
                return {};
            }
            case AstNodeKind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                const Value left = Get(state, binary.Left()),
                            right = Get(state, binary.Right());
                CheckUse(state, left, expression);
                CheckUse(state, right, expression);
                const TokenType op = binary.OperatorType();
                if (op == TokenType::And || op == TokenType::Or)
                    return Get(state, expression);
                if (op == TokenType::Equal || op == TokenType::NotEqual ||
                    op == TokenType::StrictEqual ||
                    op == TokenType::StrictNotEqual) {
                    // Distinct external pointers can alias. Only new identities
                    // are known non-null; parameter nullability is a contract.
                    const auto comparable = [&](const Value& value) {
                        if (value.kind == ValueKind::Pointer) {
                            const auto resource =
                                state.resources.find(value.identity);
                            return resource != state.resources.end() &&
                                   resource->second.owned;
                        }
                        return value.kind == ValueKind::Null ||
                               value.kind == ValueKind::Error ||
                               value.kind == ValueKind::True ||
                               value.kind == ValueKind::False ||
                               value.kind == ValueKind::Integer;
                    };
                    if (comparable(left) && comparable(right)) {
                        bool equal = left == right;
                        if (op == TokenType::NotEqual ||
                            op == TokenType::StrictNotEqual)
                            equal = !equal;
                        return {equal ? ValueKind::True : ValueKind::False};
                    }
                }
                return {};
            }
            case AstNodeKind::TupleExpression: {
                Value value{ValueKind::Aggregate};
                for (const auto& element :
                     static_cast<const core::TupleExpression&>(expression)
                         .Elements())
                    value.elements.push_back(Get(state, *element));
                return value;
            }
            case AstNodeKind::ArrayExpression: {
                Value value{ValueKind::Aggregate};
                for (const auto& element :
                     static_cast<const core::ArrayExpression&>(expression)
                         .Elements())
                    value.elements.push_back(Get(state, *element));
                return value;
            }
            case AstNodeKind::MemberAccessExpression: {
                const auto& member =
                    static_cast<const core::MemberAccessExpression&>(
                        expression);
                if (!_model.FieldFor(member)) {
                    // Only the built-in allocation error is used to refine an
                    // allocation result; unrelated error families stay unknown.
                    if (_model.TypeOf(expression).Name() == "AllocationError")
                        return {ValueKind::Error};
                    return {};
                }
                CheckUse(state, Get(state, member.Receiver()), expression,
                         member.Access() == TokenType::Arrow);
                return {};
            }
            case AstNodeKind::SliceExpression: {
                const auto& slice =
                    static_cast<const core::SliceExpression&>(expression);
                if (Owns(state, Get(state, slice.Receiver())))
                    Report(DiagnosticCode::UnverifiedOwnership, expression,
                           "cannot track allocation handles through a slice");
                return {};
            }
            case AstNodeKind::IndexExpression: {
                const auto& index =
                    static_cast<const core::IndexExpression&>(expression);
                Value receiver = Get(state, index.Receiver());
                const Value subscript = Get(state, index.Index());
                if (receiver.kind == ValueKind::Address)
                    receiver = Binding(state, receiver.address);
                CheckUse(state, receiver, expression);
                if (receiver.kind == ValueKind::Aggregate) {
                    if (subscript.kind == ValueKind::Integer &&
                        subscript.integer < receiver.elements.size())
                        return receiver.elements[subscript.integer];
                    if (Tracks(receiver))
                        Report(DiagnosticCode::UnverifiedOwnership, expression,
                               "cannot verify ownership through a dynamic "
                               "aggregate index");
                }
                return {};
            }
            default:
                return {};
        }
    }

    void Drop(State& state, const core::Expression& expression) {
        state.values.erase(&expression);
        switch (expression.Kind()) {
            case AstNodeKind::ParenthesizedExpression:
                Drop(state, static_cast<const core::ParenthesizedExpression&>(
                                expression)
                                .Inner());
                break;
            case AstNodeKind::CastExpression:
                Drop(state, static_cast<const core::CastExpression&>(expression)
                                .Value());
                break;
            case AstNodeKind::UnaryExpression:
                Drop(state,
                     static_cast<const core::UnaryExpression&>(expression)
                         .Operand());
                break;
            case AstNodeKind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                Drop(state, binary.Left());
                Drop(state, binary.Right());
                break;
            }
            case AstNodeKind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                Drop(state, assignment.Target());
                Drop(state, assignment.Value());
                break;
            }
            case AstNodeKind::CallExpression: {
                const auto& call =
                    static_cast<const core::CallExpression&>(expression);
                Drop(state, call.Callee());
                for (const auto& argument : call.Arguments())
                    Drop(state, *argument);
                break;
            }
            case AstNodeKind::MemberCallExpression: {
                const auto& call =
                    static_cast<const core::MemberCallExpression&>(expression);
                Drop(state, call.Receiver());
                for (const auto& argument : call.Arguments())
                    Drop(state, *argument);
                break;
            }
            case AstNodeKind::MemberAccessExpression:
                Drop(state, static_cast<const core::MemberAccessExpression&>(
                                expression)
                                .Receiver());
                break;
            case AstNodeKind::IndexExpression: {
                const auto& index =
                    static_cast<const core::IndexExpression&>(expression);
                Drop(state, index.Receiver());
                Drop(state, index.Index());
                break;
            }
            case AstNodeKind::TupleExpression:
                for (const auto& element :
                     static_cast<const core::TupleExpression&>(expression)
                         .Elements())
                    Drop(state, *element);
                break;
            case AstNodeKind::ArrayExpression:
                for (const auto& element :
                     static_cast<const core::ArrayExpression&>(expression)
                         .Elements())
                    Drop(state, *element);
                break;
            default:
                break;
        }
    }

    std::vector<State> Leave(
        State state, const FlowNode& from,
        const std::vector<const core::BlockStatement*>& scopes) {
        const auto leaving = [&](const core::BlockStatement* scope) {
            return std::find(from.scopes.begin(), from.scopes.end(), scope) !=
                       from.scopes.end() &&
                   std::find(scopes.begin(), scopes.end(), scope) ==
                       scopes.end();
        };
        std::vector<State> states{std::move(state)};
        // A registration is removed before invocation. Nested calls may append
        // their own defers, but Run executes those before returning here.
        while (!states.empty()) {
            const auto found = std::find_if(
                states.front().deferred.rbegin(),
                states.front().deferred.rend(),
                [&](const Deferred& item) { return leaving(item.scope); });
            if (found == states.front().deferred.rend()) break;
            const std::size_t index =
                states.front().deferred.size() - 1 -
                static_cast<std::size_t>(found -
                                         states.front().deferred.rbegin());
            std::vector<State> next;
            for (State& current : states) {
                const Deferred deferred = current.deferred[index];
                current.deferred.erase(current.deferred.begin() +
                                       static_cast<std::ptrdiff_t>(index));
                for (Outcome& outcome :
                     Invoke(*deferred.call, deferred.function,
                            deferred.arguments, std::move(current)))
                    next.push_back(std::move(outcome.state));
            }
            states = std::move(next);
        }
        return states;
    }

    std::vector<Outcome> Run(const core::FunctionDeclaration& function,
                             State initial,
                             const std::vector<Value>& arguments) {
        _active.push_back(&function);
        const ControlFlowGraph& graph = _graphs.at(&function);
        const auto savedValues = initial.values;
        for (std::size_t index = 0; index < function.Parameters().size();
             ++index)
            Bind(initial, _model.DeclaredSymbol(*function.Parameters()[index]),
                 index < arguments.size() ? arguments[index] : Value{});
        std::vector<std::vector<State>> visited(graph.nodes.size());
        std::deque<std::pair<NodeId, State>> pending;
        pending.emplace_back(graph.entry, std::move(initial));
        std::vector<Outcome> results;
        while (!pending.empty()) {
            auto [id, state] = std::move(pending.front());
            pending.pop_front();
            auto& seen = visited[id];
            if (std::find(seen.begin(), seen.end(), state) != seen.end())
                continue;
            if (++_steps > 100000 || seen.size() >= 128) {
                Report(DiagnosticCode::UnverifiedOwnership, function,
                       "ownership analysis exceeded its path budget; simplify "
                       "control flow or split this function");
                break;
            }
            seen.push_back(state);
            const FlowNode& node = graph.nodes[id];
            std::vector<State> outputs;
            if (node.memoryExpression) {
                const core::Expression& expression = *node.memoryExpression;
                const FunctionSymbol* callee = Callee(expression);
                if (expression.Kind() == AstNodeKind::CallExpression &&
                    static_cast<const core::CallExpression&>(expression)
                        .TypeArgument()) {
                    // Allocation happens before its conditional initializer.
                    const Value result = Get(state, expression);
                    // Raw pointer initializers would hide another owned
                    // allocation inside heap storage, whose teardown is not
                    // recursive through raw pointers.
                    const auto& call =
                        static_cast<const core::CallExpression&>(expression);
                    if (!call.Arguments().empty() &&
                        Owns(state, Get(state, *call.Arguments()[0])))
                        Report(DiagnosticCode::UnverifiedOwnership, expression,
                               "cannot verify an owned pointer stored inside "
                               "another allocation");
                    ForgetChildren(state, expression);
                    Put(state, expression, result);
                    outputs.push_back(std::move(state));
                } else if (expression.Kind() == AstNodeKind::CallExpression ||
                           expression.Kind() ==
                               AstNodeKind::MemberCallExpression) {
                    if (expression.Kind() ==
                        AstNodeKind::MemberCallExpression) {
                        const auto& call =
                            static_cast<const core::MemberCallExpression&>(
                                expression);
                        if (!callee && call.Member() != "len" &&
                            Owns(state, Get(state, call.Receiver())))
                            Report(DiagnosticCode::UnverifiedOwnership,
                                   expression,
                                   "cannot track allocation handles through "
                                   "this container operation");
                        if (call.Access() == TokenType::Arrow)
                            CheckUse(state, Get(state, call.Receiver()),
                                     expression, true);
                    }
                    const std::vector<Value> arguments =
                        Arguments(state, expression);
                    for (Outcome& outcome : Invoke(
                             expression, callee, arguments, std::move(state))) {
                        ForgetChildren(outcome.state, expression);
                        Put(outcome.state, expression,
                            std::move(outcome.result));
                        outputs.push_back(std::move(outcome.state));
                    }
                } else {
                    Value result = Evaluate(state, expression);
                    ForgetChildren(state, expression);
                    Put(state, expression, std::move(result));
                    outputs.push_back(std::move(state));
                }
            } else {
                if (node.memoryUntrackedStore) {
                    if (Owns(state, Get(state, *node.memoryUntrackedStore)))
                        Report(DiagnosticCode::UnverifiedOwnership,
                               *node.memoryUntrackedStore,
                               "cannot verify ownership stored in a structure "
                               "field initializer");
                    Drop(state, *node.memoryUntrackedStore);
                    CheckLost(state, *node.memoryUntrackedStore);
                }
                if (node.memoryDeclaration) {
                    const auto& declaration = *node.memoryDeclaration;
                    Value value;
                    if (declaration.Initializer())
                        value = Get(state, *declaration.Initializer());
                    CheckUse(state, value, declaration);
                    Bind(state, _model.DeclaredSymbol(declaration),
                         std::move(value));
                    if (declaration.Initializer())
                        Drop(state, *declaration.Initializer());
                    CheckLost(state, declaration);
                }
                if (node.memoryDestructure) {
                    const auto& destructure = *node.memoryDestructure;
                    const Value tuple = Get(state, destructure.Value());
                    CheckUse(state, tuple, destructure);
                    for (std::size_t index = 0;
                         index < destructure.Bindings().size(); ++index)
                        Bind(state,
                             _model.DestructureTarget(
                                 *destructure.Bindings()[index]),
                             index < tuple.elements.size()
                                 ? tuple.elements[index]
                                 : Value{});
                    Drop(state, destructure.Value());
                    CheckLost(state, destructure);
                }
                if (node.memoryDefer) {
                    const core::Expression& call = node.memoryDefer->Call();
                    std::vector<Value> arguments = Arguments(state, call);
                    for (const Value& value : arguments)
                        CheckUse(state, value, call);
                    // Even calls without tracked arguments may allocate; they
                    // need execution at cleanup rather than at registration.
                    if (Relevant(Callee(call), arguments)) {
                        if (state.deferred.size() >= 128) {
                            Report(DiagnosticCode::UnverifiedOwnership, call,
                                   "cannot verify an unbounded number of "
                                   "deferred pointer operations");
                            continue;
                        }
                        state.deferred.push_back({node.scopes.back(), &call,
                                                  Callee(call),
                                                  std::move(arguments)});
                    }
                    Drop(state, call);
                    CheckLost(state, *node.memoryDefer);
                }
                if (node.memoryDiscard) {
                    Drop(state, *node.memoryDiscard);
                    CheckLost(state, *node.memoryDiscard);
                }
                if (node.memoryBoolean)
                    Put(state, *node.memoryBoolean,
                        {node.booleanValue ? ValueKind::True
                                           : ValueKind::False});
                outputs.push_back(std::move(state));
            }
            for (State& output : outputs) {
                if (node.successors.empty()) {
                    Value result;
                    if (node.memoryReturn && node.memoryReturn->Value()) {
                        result = Get(output, *node.memoryReturn->Value());
                        Drop(output, *node.memoryReturn->Value());
                    }
                    const core::AstNode& at =
                        node.memoryReturn ? static_cast<const core::AstNode&>(
                                                *node.memoryReturn)
                                          : function.Body();
                    // The synthetic fallthrough exit has no scopes. Usually
                    // cleanup already ran on its incoming edge.
                    for (State& finished : Leave(std::move(output), node, {})) {
                        for (const auto& [symbol, scope] : graph.symbolScopes)
                            finished.bindings.erase(symbol);
                        for (const auto& parameter : function.Parameters())
                            finished.bindings.erase(
                                _model.DeclaredSymbol(*parameter));
                        std::erase_if(finished.values, [&](const auto& entry) {
                            return !savedValues.contains(entry.first);
                        });
                        CheckUse(finished, result, at);
                        CheckLost(finished, at, result);
                        results.push_back({std::move(finished), result});
                    }
                    continue;
                }
                const Value condition =
                    node.condition ? Get(output, *node.condition) : Value{};
                for (std::size_t edge = 0; edge < node.successors.size();
                     ++edge) {
                    if (node.condition &&
                        ((condition.kind == ValueKind::True && edge != 0) ||
                         (condition.kind == ValueKind::False && edge != 1)))
                        continue;
                    State outgoing = output;
                    if (node.condition) Drop(outgoing, *node.condition);
                    if (node.allocation) {
                        if (edge == 0) {
                            if (!Allocate(outgoing, *node.allocation)) continue;
                        } else {
                            Value failure{ValueKind::Aggregate};
                            failure.elements = {{ValueKind::Null},
                                                {ValueKind::Error}};
                            Put(outgoing, *node.allocation, std::move(failure));
                        }
                    }
                    const NodeId successor = node.successors[edge];
                    for (State& next : Leave(std::move(outgoing), node,
                                             graph.nodes[successor].scopes)) {
                        for (const auto& [symbol, scope] : graph.symbolScopes)
                            if (std::find(graph.nodes[successor].scopes.begin(),
                                          graph.nodes[successor].scopes.end(),
                                          scope) ==
                                graph.nodes[successor].scopes.end())
                                next.bindings.erase(symbol);
                        // Do not check here while a return value is in transit:
                        // its expression temporary represents the transferred
                        // owner.
                        const core::AstNode& loss =
                            node.memoryExpression
                                ? static_cast<const core::AstNode&>(
                                      *node.memoryExpression)
                            : node.condition
                                ? static_cast<const core::AstNode&>(
                                      *node.condition)
                                : function.Body();
                        CheckLost(next, loss);
                        pending.emplace_back(successor, std::move(next));
                    }
                }
            }
        }
        _active.pop_back();
        return results;
    }

    bool Owns(const State& state, const Value& value,
              std::size_t depth = 0) const {
        if (depth > 16) return true;
        const auto resource = state.resources.find(value.identity);
        if (resource != state.resources.end() && resource->second.owned)
            return true;
        if (value.address &&
            Owns(state, Binding(state, value.address), depth + 1))
            return true;
        return std::any_of(
            value.elements.begin(), value.elements.end(),
            [&](const Value& child) { return Owns(state, child, depth + 1); });
    }

    const SemanticModel& _model;
    diagnostics::DiagnosticBag& _diagnostics;
    std::map<const FunctionSymbol*, const core::FunctionDeclaration*>
        _functions;
    std::map<const core::FunctionDeclaration*, ControlFlowGraph> _graphs;
    std::vector<const core::FunctionDeclaration*> _ordered;
    std::set<const core::FunctionDeclaration*> _resourceFunctions;
    std::vector<const core::FunctionDeclaration*> _active;
    std::set<std::pair<DiagnosticCode, std::size_t>> _reported;
    std::size_t _steps = 0;
};
}  // namespace

void AnalyzeAllocations(const core::CompilationUnit& unit,
                        const SemanticModel& model,
                        diagnostics::DiagnosticBag& diagnostics) {
    AllocationChecker(unit, model, diagnostics).Analyze();
}
}  // namespace flecha::semantic
