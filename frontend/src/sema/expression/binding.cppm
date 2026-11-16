module;

#include <memory>
#include <string>

export module zep.frontend.sema.expression.binding;

import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.lifecycle;
import zep.frontend.sema.expression.values;
import zep.frontend.sema.scopes.environment;
import zep.frontend.sema.scopes.scope;
import zep.common.diagnostic.collection;

export class Binding {
  private:
    NodeArena& nodes;
    Lifecycle& lifecycle;

  public:
    Binding(NodeArena& nodes, Lifecycle& lifecycle) : nodes(nodes), lifecycle(lifecycle) {}

    Expression* convert(Expression* value, const Type* target, TypeResolver& resolver) {
        if (value == nullptr || value->type == nullptr || target == nullptr) {
            return value;
        }

        const auto* resolved = resolver.resolve_type(target);
        const auto* source = resolver.resolve_type(value->type);
        const auto* reference = resolved != nullptr ? resolved->as<PointerType>() : nullptr;

        if (reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None &&
            !Type::same(value->type, resolved)) {
            const auto* pointer = value->type->as<PointerType>();

            if (pointer != nullptr && pointer->reference_kind != PointerType::ReferenceKind::Type::None) {
                return value;
            }

            auto* address = nodes.create<UnaryExpression>(value->span,
                reference->is_mutable ? UnaryOperator::Type::AddressOfMut : UnaryOperator::Type::AddressOf,
                value);
            address->type = resolved;

            return address;
        }

        const auto conversion = Compatibility::coercion(resolved, source);

        if (conversion == Coercion::Type::None) {
            return value;
        }

        auto* result = nodes.create<CoerceExpression>(value->span, value, target, conversion, value->type);

        if (conversion != Coercion::Type::InterfaceValue) {
            return result;
        }

        if (const auto* pointer = source->as<PointerType>(); pointer != nullptr) {
            source = resolver.resolve_type(pointer->element);
        }

        const auto* structure = source->as<StructType>();
        const auto* interface = resolved->as<InterfaceType>();
        result->implementations.reserve(interface->methods.size());

        for (const auto& method : interface->methods) {
            const auto* expected = resolver.method_type(method.type, structure);

            for (const auto* owner = structure; owner != nullptr; owner = owner->base_type) {
                const auto* members = Scope::members(owner);
                const auto* overloads = members != nullptr
                    ? members->find_local_function_overloads(method.name) : nullptr;

                if (overloads == nullptr) {
                    continue;
                }

                for (const auto* function : *overloads) {
                    const auto* signature = resolver.method_type(function->signature(), owner);

                    if (expected->matches_method(*signature)) {
                        result->implementations.emplace_back(*function, signature);
                        break;
                    }
                }

                if (result->implementations.size() == method.index + 1) {
                    break;
                }
            }
        }

        return result;
    }

    Expression* initialize(Expression* value, const Type* target, TypeResolver& resolver,
                           Diagnostics& diagnostics) {
        return construct(convert(value, target, resolver), target, resolver, diagnostics);
    }

    void reference(const Expression& value, const Type* target, const SymbolEnvironment& environment,
                   Diagnostics& diagnostics) const {
        const auto* reference = target != nullptr ? target->as<PointerType>() : nullptr;

        if (reference == nullptr || reference->reference_kind == PointerType::ReferenceKind::Type::None) {
            return;
        }

        if (const auto* source = value.type != nullptr && value.as<NullLiteral>() == nullptr
                ? value.type->as<PointerType>() : nullptr;
            source != nullptr && source->reference_kind != PointerType::ReferenceKind::Type::None) {
            return;
        }

        if (value.as<NullLiteral>() != nullptr) {
            diagnostics.add_error(value.span, "references cannot bind null");
        } else if (reference->is_mutable && !Values::mutable_place(value, environment) &&
                   !Values::moving(value) &&
                   (reference->reference_kind != PointerType::ReferenceKind::Type::Move || Values::place(value))) {
            diagnostics.add_error(value.span, "mutable references require a mutable place");
        }
    }

    Expression* construct(Expression* value, const Type* target, TypeResolver& resolver,
                          Diagnostics& diagnostics, Operation::Type operation = Operation::Type::Copy) {
        if (value == nullptr || target == nullptr) {
            return value;
        }

        if (const auto* coercion = value->as<CoerceExpression>(); coercion != nullptr &&
            coercion->operation != nullptr) {
            return value;
        }

        const auto moving = operation == Operation::Type::Move || Values::moving(*value);

        if (!Values::place(*value) && !moving) {
            return value;
        }

        const auto* type = resolver.resolve_type(target);

        if (type != nullptr && value->type != nullptr &&
            (type->is<NamedType>() || TypeLayout().resolve(type).state == TypeLayout::Result::State::Dependent)) {
            type = resolver.resolve_type(value->type);
        }

        auto plan = lifecycle.resolve(type, moving ? Operation::Type::Move : Operation::Type::Copy,
                                      resolver);

        if (plan->action == LifecyclePlan::Action::Type::Deleted) {
            diagnostics.add_error(value->span, plan->function != nullptr
                ? "use of deleted constructor for '" + type->to_string() + "'"
                : "type '" + type->to_string() + "' is not copyable");

            return value;
        }

        if (plan->action == LifecyclePlan::Action::Type::Trivial &&
            (type == nullptr || (type->as_nominal() == nullptr && !type->is<ArrayType>()))) {
            return value;
        }

        auto* result = nodes.create<CoerceExpression>(value->span, value,
            type, plan->operation == Operation::Type::Move ? Coercion::Type::MoveValue : Coercion::Type::CopyValue,
            value->type);
        result->operation = std::move(plan);

        return result;
    }
};
