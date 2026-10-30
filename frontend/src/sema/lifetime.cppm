module;

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.frontend.sema.lifetime;

import zep.frontend.node;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type_resolver;

export class Lifetime;

class LifetimeInfo {
  public:
    class InitializationState {
      private:
        friend class Lifetime;

        std::unordered_set<const VariableSymbol*> variables;

        explicit InitializationState(
            std::unordered_set<const VariableSymbol*> variables)
            : variables(std::move(variables)) {}
    };

    const FunctionSymbol* copy_constructor = nullptr;
    const FunctionSymbol* destructor = nullptr;
};

export class Lifetime {
  private:
    TypeArena& types;
    NodeArena& nodes;

    std::unordered_map<const NominalType*, LifetimeInfo> entries;
    std::unordered_set<const VariableSymbol*> destroyed_variables;

    static const NominalType* identity(const NominalType* type) {
        return type != nullptr && type->definition != nullptr ? type->definition : type;
    }

    static bool is_place(const Expression& expression) {
        if (expression.as<IdentifierExpression>() != nullptr ||
            expression.as<MemberExpression>() != nullptr ||
            expression.as<IndexExpression>() != nullptr) {
            return true;
        }

        const auto* unary = expression.as<UnaryExpression>();
        return unary != nullptr &&
               unary->op == UnaryExpression::Operator::Type::Dereference;
    }

  public:
    class FunctionScope {
      private:
        Lifetime& lifetime;
        std::unordered_set<const VariableSymbol*> previous;

      public:
        explicit FunctionScope(Lifetime& lifetime)
            : lifetime(lifetime), previous(std::move(lifetime.destroyed_variables)) {
            lifetime.destroyed_variables.clear();
        }

        FunctionScope(const FunctionScope&) = delete;
        FunctionScope& operator=(const FunctionScope&) = delete;

        ~FunctionScope() {
            lifetime.destroyed_variables = std::move(previous);
        }
    };

    Lifetime(TypeArena& types, NodeArena& nodes) : types(types), nodes(nodes) {}

    [[nodiscard]] FunctionScope enter_function() {
        return FunctionScope(*this);
    }

    LifetimeInfo::InitializationState checkpoint() const {
        return LifetimeInfo::InitializationState(destroyed_variables);
    }

    void restore(LifetimeInfo::InitializationState state) {
        destroyed_variables = std::move(state.variables);
    }

    void join(LifetimeInfo::InitializationState first,
              LifetimeInfo::InitializationState second) {
        destroyed_variables = std::move(first.variables);
        destroyed_variables.insert(second.variables.begin(), second.variables.end());
    }

    void register_copy_constructor(const NominalType* type, const FunctionSymbol* function) {
        entries[identity(type)].copy_constructor = function;
    }

    void register_destructor(const NominalType* type, const FunctionSymbol* function) {
        entries[identity(type)].destructor = function;
    }

    const FunctionSymbol* find_copy_constructor(const NominalType* type) const {
        const auto iterator = entries.find(identity(type));
        return iterator != entries.end() ? iterator->second.copy_constructor : nullptr;
    }

    const FunctionSymbol* find_destructor(const NominalType* type) const {
        const auto iterator = entries.find(identity(type));
        return iterator != entries.end() ? iterator->second.destructor : nullptr;
    }

    bool needs_destruction(const Type* type, TypeResolver& resolver) const {
        if (type == nullptr) {
            return false;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* actual = resolved != nullptr ? resolved : type;

        if (const auto* array = actual->as<ArrayType>(); array != nullptr) {
            return needs_destruction(array->element, resolver);
        }

        if (const auto* structure = actual->as<StructType>(); structure != nullptr) {
            if (find_destructor(structure) != nullptr) {
                return true;
            }

            for (const auto& field : structure->fields) {
                if (needs_destruction(field.type, resolver)) {
                    return true;
                }
            }

            return structure->base_type != nullptr &&
                   needs_destruction(structure->base_type, resolver);
        }

        if (const auto* enumeration = actual->as<EnumType>(); enumeration != nullptr) {
            for (const auto& variant : enumeration->variants) {
                for (const auto& field : variant.fields) {
                    if (needs_destruction(field.type, resolver)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool is_destroyed(const VariableSymbol* variable) const {
        return destroyed_variables.contains(variable);
    }

    bool mark_destroyed(const VariableSymbol* variable) {
        return destroyed_variables.insert(variable).second;
    }

    void mark_initialized(const VariableSymbol* variable) {
        destroyed_variables.erase(variable);
    }

    Expression* copy_construct(Expression* value, const Type* target_type,
                               TypeResolver& resolver) {
        if (value == nullptr || target_type == nullptr || !is_place(*value)) {
            return value;
        }

        const auto* resolved = resolver.resolve_type(target_type);
        const auto* nominal = resolved != nullptr ? resolved->as_nominal() : nullptr;
        const auto* constructor = find_copy_constructor(nominal);
        if (constructor == nullptr || constructor->function_type == nullptr) {
            return value;
        }

        const auto* pointer_type = types.create<PointerType>(resolved, false);
        auto* address = nodes.create<UnaryExpression>(
            value->span, UnaryExpression::Operator::Type::AddressOf, value);
        address->type = pointer_type;

        std::vector<Argument*> arguments;
        arguments.reserve(1);
        arguments.push_back(nodes.create<Argument>(value->span, std::string(), address));

        auto* callee = nodes.create<IdentifierExpression>(value->span, nominal->name);
        callee->type = constructor->function_type;

        auto* call = nodes.create<CallExpression>(value->span, callee,
                                                  std::vector<GenericArgument*>(),
                                                  std::move(arguments));
        call->resolved_target = std::make_unique<DirectCallTarget>(*constructor);
        call->type = resolved;
        return call;
    }
};
