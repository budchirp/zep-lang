module;

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.lifecycle;

export import zep.frontend.sema.lifecycle.operation;

import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types.resolver;

export class Lifecycle {
  private:
    class Key {
      public:
        const Type* type;
        Operation::Type operation;
        LifecycleMode::Type mode;

        Key(const Type* type, Operation::Type operation, LifecycleMode::Type mode)
            : type(type), operation(operation), mode(mode) {}

        bool operator==(const Key& other) const = default;
    };

    class Hash {
      public:
        std::size_t operator()(const Key& key) const {
            return std::hash<const Type*>{}(key.type) ^
                   (static_cast<std::size_t>(key.operation) << 1) ^
                   (static_cast<std::size_t>(key.mode) << 4);
        }
    };

    std::unordered_map<const NominalType*, std::array<const FunctionSymbol*, 5>> members;
    std::unordered_map<Key, std::shared_ptr<const LifecyclePlan>, Hash> plans;
    std::unordered_set<const FunctionSymbol*> disabled;

    static const NominalType* identity(const NominalType* type) {
        return type != nullptr && type->definition != nullptr ? type->definition : type;
    }

  public:
    Lifecycle() : members(), plans(), disabled() {}

    void disable(const FunctionSymbol* function) {
        disabled.insert(function);
        plans.clear();
    }

    bool deleted(const FunctionSymbol* function) const {
        return function != nullptr && (function->is_deleted || disabled.contains(function));
    }

    void register_function(const NominalType* type, Operation::Type operation,
                           const FunctionSymbol* function) {
        members[identity(type)][static_cast<std::size_t>(operation)] = function;
        plans.clear();
    }

    const FunctionSymbol* function(const NominalType* type, Operation::Type operation) const {
        const auto* definition = identity(type);

        if (const auto found = members.find(definition); found != members.end()) {
            if (const auto* result = found->second[static_cast<std::size_t>(operation)]; result != nullptr) {
                return result;
            }
        }

        if (operation != Operation::Type::Assign && operation != Operation::Type::MoveAssign) {
            return nullptr;
        }

        auto* scope = Scope::members(definition);

        if (scope == nullptr) {
            return nullptr;
        }

        for (const auto* candidate : scope->lookup_function_overloads("operator=")) {
            if (candidate->signature()->parameters.size() != 2) {
                continue;
            }

            const auto* source = candidate->signature()->parameters.back().type->as<PointerType>();
            const auto moving = source != nullptr &&
                source->reference_kind == PointerType::ReferenceKind::Type::Move;

            if (source != nullptr && moving == (operation == Operation::Type::MoveAssign)) {
                return candidate;
            }
        }

        return nullptr;
    }

    std::shared_ptr<const LifecyclePlan> resolve(const Type* type, Operation::Type operation,
                                                TypeResolver& resolver,
                                                LifecycleMode::Type mode = LifecycleMode::Type::Selected) {
        const auto* resolved = resolver.resolve_type(type);

        if (resolved == nullptr) {
            resolved = type;
        }

        Key key(resolved, operation, mode);

        if (const auto found = plans.find(key); found != plans.end()) {
            return found->second;
        }

        auto action = LifecyclePlan::Action::Type::Trivial;
        const auto* nominal = resolved != nullptr ? resolved->as_nominal() : nullptr;
        const auto* selected = mode == LifecycleMode::Type::Selected ? function(nominal, operation) : nullptr;
        std::vector<LifecyclePart> parts;

        if (resolved != nullptr && (resolved->is<NamedType>() ||
            (nominal != nullptr && !nominal->generic_parameters.empty()) ||
            (resolved->is<ArrayType>() && !std::holds_alternative<ConcreteArrayExtent>(resolved->as<ArrayType>()->extent)))) {
            action = LifecyclePlan::Action::Type::Deferred;
        } else if (deleted(selected)) {
            action = LifecyclePlan::Action::Type::Deleted;
        } else if (selected != nullptr && operation != Operation::Type::Destroy) {
            action = LifecyclePlan::Action::Type::Function;
        } else if (mode == LifecycleMode::Type::Selected && operation == Operation::Type::Move &&
                   (function(nominal, Operation::Type::Copy) != nullptr ||
                    function(nominal, Operation::Type::Destroy) != nullptr ||
                    function(nominal, Operation::Type::Assign) != nullptr ||
                    function(nominal, Operation::Type::MoveAssign) != nullptr)) {
            return resolve(resolved, Operation::Type::Copy, resolver);
        } else if (mode == LifecycleMode::Type::Selected && operation == Operation::Type::Copy &&
                   (function(nominal, Operation::Type::Move) != nullptr ||
                    function(nominal, Operation::Type::MoveAssign) != nullptr)) {
            action = LifecyclePlan::Action::Type::Deleted;
        } else {
            if (const auto* structure = resolved != nullptr ? resolved->as<StructType>() : nullptr;
                structure != nullptr) {
                const auto inherited = structure->base_type != nullptr ? structure->base_type->fields.size() : 0;
                parts.reserve(structure->fields.size() - inherited + 1);

                if (structure->base_type != nullptr) {
                    parts.emplace_back(LifecyclePart::Kind::Type::Base, std::string(), 0, 1,
                                       resolve(structure->base_type, operation, resolver));
                }

                for (auto index = inherited; index < structure->fields.size(); ++index) {
                    const auto& field = structure->fields[index];
                    parts.emplace_back(LifecyclePart::Kind::Type::Field, field.name, 0, 1,
                                       resolve(field.type, operation, resolver));
                }
            } else if (const auto* array = resolved != nullptr ? resolved->as<ArrayType>() : nullptr;
                       array != nullptr) {
                parts.reserve(1);
                parts.emplace_back(LifecyclePart::Kind::Type::Element, std::string(), 0,
                                   std::get<ConcreteArrayExtent>(array->extent).value,
                                   resolve(array->element, operation, resolver));
            } else if (const auto* enumeration = resolved != nullptr ? resolved->as<EnumType>() : nullptr;
                       enumeration != nullptr && enumeration->backing_type == nullptr) {
                auto count = std::size_t{0};

                for (const auto& variant : enumeration->variants) {
                    count += variant.fields.size();
                }

                parts.reserve(count);

                for (const auto& variant : enumeration->variants) {
                    for (const auto& field : variant.fields) {
                        parts.emplace_back(LifecyclePart::Kind::Type::Payload, field.name,
                                           variant.index, 1, resolve(field.type, operation, resolver));
                    }
                }
            }

            if (selected != nullptr || !parts.empty()) {
                action = LifecyclePlan::Action::Type::Aggregate;
            }

            if (operation == Operation::Type::Destroy) {
                auto owns = selected != nullptr;

                for (const auto& part : parts) {
                    owns = owns || part.plan->action != LifecyclePlan::Action::Type::Trivial;
                }

                if (!owns) {
                    action = LifecyclePlan::Action::Type::Trivial;
                }
            } else {
                for (const auto& part : parts) {
                    if (part.plan->action == LifecyclePlan::Action::Type::Deleted) {
                        action = LifecyclePlan::Action::Type::Deleted;
                        break;
                    }
                }
            }
        }

        auto result = std::make_shared<const LifecyclePlan>(resolved, operation, action, selected,
                                                          std::move(parts));
        plans.emplace(key, result);

        return result;
    }

    bool available(const Type* type, Operation::Type operation, TypeResolver& resolver,
                   LifecycleMode::Type mode = LifecycleMode::Type::Selected) {
        return resolve(type, operation, resolver, mode)->action != LifecyclePlan::Action::Type::Deleted;
    }

    bool needs_drop(const Type* type, TypeResolver& resolver) {
        return resolve(type, Operation::Type::Destroy, resolver)->action != LifecyclePlan::Action::Type::Trivial;
    }
};
