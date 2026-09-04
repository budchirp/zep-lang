module;

#include <string>

export module zep.frontend.sema.resolver.operator_resolver;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class OperatorResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& type_resolver;

    bool require_numeric(Expression& expression, const Type* type, const std::string& side,
                         const std::string& operator_kind) {
        if (type->is_numeric()) {
            return true;
        }

        context.diagnostics.add_error(expression.span, side + " operand of " + operator_kind +
                                                           " operator must be numeric, got '" +
                                                           type->to_string() + "'");

        return false;
    }

    bool require_compatible(BinaryExpression& node, const Type* left_type, const Type* right_type,
                            const std::string& operation) {
        if (left_type->accepts(right_type)) {
            return true;
        }

        context.diagnostics.add_error(node.span, "type mismatch in " + operation + ": '" +
                                                     left_type->to_string() + "' and '" +
                                                     right_type->to_string() + "'");

        return false;
    }

    void validate_arithmetic(BinaryExpression& node, const Type* left_type,
                             const Type* right_type) {
        if (!require_numeric(*node.left, left_type, "left", "arithmetic")) {
            return;
        }

        if (!require_numeric(*node.right, right_type, "right", "arithmetic")) {
            return;
        }

        if (!require_compatible(node, left_type, right_type, "arithmetic")) {
            return;
        }

        node.type = left_type;
    }

    void validate_numeric_comparison(BinaryExpression& node, const Type* left_type,
                                     const Type* right_type) {
        if (!require_numeric(*node.left, left_type, "left", "comparison")) {
            return;
        }

        if (!require_numeric(*node.right, right_type, "right", "comparison")) {
            return;
        }

        if (!require_compatible(node, left_type, right_type, "comparison")) {
            return;
        }

        node.type = sema.types.create<BooleanType>();
    }

    void validate_logical(BinaryExpression& node, const Type* left_type, const Type* right_type) {
        if (left_type->is<BooleanType>() && right_type->is<BooleanType>()) {
            node.type = sema.types.create<BooleanType>();
            return;
        }

        context.diagnostics.add_error(node.span, "type mismatch in logical operator: '" +
                                                     left_type->to_string() + "' and '" +
                                                     right_type->to_string() + "'");
    }

  public:
    explicit OperatorResolver(Context& context, SemaContext& sema, TypeResolver& type_resolver)
        : context(context), sema(sema), type_resolver(type_resolver) {}

    void resolve(BinaryExpression& node) {
        using Operator = BinaryExpression::Operator::Type;

        const auto* left_type = node.left->type;
        const auto* right_type = node.right->type;

        if (left_type == nullptr || right_type == nullptr) {
            return;
        }

        switch (node.op) {
        case Operator::As:
        case Operator::Is:
            if (node.op == BinaryExpression::Operator::Type::As) {
                node.type = type_resolver.resolve_type(right_type);
                return;
            }

            node.type = sema.types.create<BooleanType>();
            break;
        case Operator::Plus:
        case Operator::Minus:
        case Operator::Asterisk:
        case Operator::Divide:
        case Operator::Modulo:
            validate_arithmetic(node, left_type, right_type);
            break;
        case Operator::LessThan:
        case Operator::GreaterThan:
        case Operator::LessEqual:
        case Operator::GreaterEqual:
            validate_numeric_comparison(node, left_type, right_type);
            break;
        case Operator::Equals:
        case Operator::NotEquals:
            if (!require_compatible(node, left_type, right_type, "comparison")) {
                return;
            }

            node.type = sema.types.create<BooleanType>();
            break;
        case Operator::And:
        case Operator::Or:
            validate_logical(node, left_type, right_type);
            break;
        default:
            break;
        }
    }
};
