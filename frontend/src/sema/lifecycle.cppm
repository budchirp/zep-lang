module;

#include <memory>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.lifecycle;

import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class LifecycleResolver {
  private:
    SemaContext& sema;
    TypeResolver& resolver;

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
    LifecycleResolver(SemaContext& sema, TypeResolver& resolver)
        : sema(sema), resolver(resolver) {}

    Expression* copy_construct(Expression* value, const Type* target_type) {
        if (value == nullptr || target_type == nullptr || !is_place(*value)) {
            return value;
        }

        const auto* resolved = resolver.resolve_type(target_type);
        const auto* nominal = resolved != nullptr ? resolved->as_nominal() : nullptr;
        const auto* lifecycle = sema.lifecycles.find(nominal);
        const auto* constructor = lifecycle != nullptr ? lifecycle->copy_constructor : nullptr;
        if (constructor == nullptr || constructor->function_type == nullptr) {
            return value;
        }

        const auto* pointer_type = sema.types.create<PointerType>(resolved, false);
        auto* address = sema.nodes.create<UnaryExpression>(
            value->span, UnaryExpression::Operator::Type::AddressOf, value);
        address->type = pointer_type;

        std::vector<Argument*> arguments;
        arguments.reserve(1);
        arguments.push_back(sema.nodes.create<Argument>(value->span, std::string(), address));

        auto* callee = sema.nodes.create<IdentifierExpression>(value->span, nominal->name);
        callee->type = constructor->function_type;

        auto* call = sema.nodes.create<CallExpression>(value->span, callee,
                                                       std::vector<GenericArgument*>(),
                                                       std::move(arguments));
        call->resolved_target = std::make_unique<DirectCallTarget>(*constructor);
        call->type = resolved;
        return call;
    }
};
