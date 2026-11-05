module;

#include <memory>
#include <string>

export module zep.frontend.sema.expression.binding;

import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.lifecycle;
import zep.frontend.sema.expression.values;
import zep.frontend.sema.scopes.environment;
import zep.common.diagnostic.collection;

export class Binding {
  private:
    NodeArena& nodes;
    Lifecycle& lifecycle;

  public:
    Binding(NodeArena& nodes, Lifecycle& lifecycle) : nodes(nodes), lifecycle(lifecycle) {}

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
