module;

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module zep.frontend.sema.constants.evaluator;

import zep.common.source.span;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.constants.math;
import zep.frontend.sema.generics.environment;
import zep.frontend.sema.scopes.environment;
import zep.frontend.kind;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;

export class EvaluationPolicy {
  public:
    enum class Type : std::uint8_t {
        Required,
        Probe,
        Dependent,
    };
};

export class EvaluationResult {
  public:
    enum class State : std::uint8_t {
        Concrete,
        Dependent,
        Invalid,
    };

    State state;
    std::optional<CompileTimeValue> value;

    EvaluationResult(State state, std::optional<CompileTimeValue> value = std::nullopt)
        : state(state), value(std::move(value)) {}
};

export class Evaluator : public ExpressionVisitor<std::optional<CompileTimeValue>> {
  private:
    Diagnostics& diagnostics;
    Diagnostics probe_diagnostics;

    SymbolEnvironment* env;
    const CompileTimeEnvironment* environment;
    EvaluationPolicy::Type policy;
    bool dependent;

    Diagnostics& output() {
        return policy == EvaluationPolicy::Type::Probe ? probe_diagnostics : diagnostics;
    }

    std::optional<CompileTimeValue> evaluate_value(Expression& node) {
        if (environment != nullptr || policy != EvaluationPolicy::Type::Required) {
            return visit_expression(node);
        }

        if (node.compile_time_value.has_value()) {
            return node.compile_time_value;
        }

        auto result = visit_expression(node);
        node.compile_time_value = result;
        return result;
    }

    std::optional<CompileTimeValue> integer_error(Span span, const std::string& message) {
        output().add_error(span, message);
        return std::nullopt;
    }

    std::optional<CompileTimeValue> integer_literal(NumberLiteral& node, bool negative = false) {
        const auto* type = node.type;
        if (type == nullptr) {
            return integer_error(node.span, "constant integer has no resolved type");
        }
        std::string_view digits(node.value);
        auto base = 10;
        if (digits.starts_with("0x") || digits.starts_with("0X")) {
            base = 16;
            digits.remove_prefix(2);
        } else if (digits.starts_with("0b") || digits.starts_with("0B")) {
            base = 2;
            digits.remove_prefix(2);
        }

        auto magnitude = std::uint64_t{0};
        const auto parsed =
            std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base);
        if (parsed.ec != std::errc() || parsed.ptr != digits.data() + digits.size()) {
            return integer_error(node.span, "constant integer is out of range for '" +
                                                (type != nullptr ? type->to_string() : "unknown") +
                                                "'");
        }

        const auto value = static_cast<__int128>(magnitude);
        return ConstantMath::integer(output(), negative ? -value : value, type,
                                                 node.span);
    }

    std::optional<bool> read_boolean(CompileTimeValue value, Span span) {
        if (value.kind != CompileTimeValue::Kind::Type::Boolean) {
            output().add_error(span, "constant expression requires a concrete boolean value");
            return std::nullopt;
        }

        return std::get<bool>(value.payload);
    }

  public:
    Evaluator(Diagnostics& diagnostics, SymbolEnvironment* env = nullptr,
              const CompileTimeEnvironment* environment = nullptr,
              EvaluationPolicy::Type policy = EvaluationPolicy::Type::Required)
        : diagnostics(diagnostics), env(env), environment(environment),
          policy(policy), dependent(false) {}

    EvaluationResult evaluate(Expression& node) {
        dependent = false;

        auto value = evaluate_value(node);
        if (value.has_value()) {
            return EvaluationResult(EvaluationResult::State::Concrete, std::move(value));
        }

        return EvaluationResult(dependent
                                    ? EvaluationResult::State::Dependent
                                    : EvaluationResult::State::Invalid);
    }

    std::optional<CompileTimeValue> visit(NumberLiteral& node) override {
        return integer_literal(node);
    }

    std::optional<CompileTimeValue> visit(FloatLiteral& node) override {
        output().add_error(node.span, "float literals not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(StringLiteral& node) override {
        return CompileTimeValue(CompileTimeValue::Kind::Type::String, node.type, node.value);
    }

    std::optional<CompileTimeValue> visit(CharLiteral& node) override {
        return CompileTimeValue(CompileTimeValue::Kind::Type::Char, node.type, node.value);
    }

    std::optional<CompileTimeValue> visit(BooleanLiteral& node) override {
        return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type, node.value);
    }

    std::optional<CompileTimeValue> visit(NullLiteral& node) override {
        return CompileTimeValue(CompileTimeValue::Kind::Type::Null, node.type);
    }

    std::optional<CompileTimeValue> visit(IdentifierExpression& node) override {
        if (environment != nullptr) {
            const auto* binding = node.generic_declaration != nullptr
                                      ? environment->lookup(node.generic_declaration)
                                      : environment->lookup(node.name);
            const auto* value = binding != nullptr ? std::get_if<ConstBinding>(binding) : nullptr;
            if (value != nullptr && value->value.has_value()) {
                return *value->value;
            }
            if (value != nullptr) {
                if (policy == EvaluationPolicy::Type::Dependent) {
                    dependent = true;
                    return std::nullopt;
                }
                return integer_error(node.span,
                                     "constant expression requires a concrete generic argument '" +
                                         node.name + "'");
            }
        }

        if (node.generic_declaration != nullptr && policy == EvaluationPolicy::Type::Dependent) {
            dependent = true;
            return std::nullopt;
        }

        output().add_error(node.span,
                              "constant expression cannot use runtime value '" + node.name + "'");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BinaryExpression& node) override {
        if (node.op == BinaryExpression::Operator::Type::As) {
            auto value = evaluate_value(*node.left);
            if (!value.has_value()) {
                return std::nullopt;
            }
            return ConstantMath::cast(output(), *value, node.right->type, node.span);
        }

        auto left = evaluate_value(*node.left);
        if (!left.has_value()) {
            return std::nullopt;
        }

        using Operator = BinaryExpression::Operator::Type;

        if (node.op == Operator::And || node.op == Operator::Or) {
            auto boolean = read_boolean(*left, node.left->span);
            if (!boolean.has_value()) {
                return std::nullopt;
            }

            if ((node.op == Operator::And && !*boolean) || (node.op == Operator::Or && *boolean)) {
                return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type, *boolean);
            }
        }

        auto right = evaluate_value(*node.right);
        if (!right.has_value()) {
            return std::nullopt;
        }

        if (node.op == Operator::And || node.op == Operator::Or) {
            auto boolean = read_boolean(*right, node.right->span);
            if (!boolean.has_value()) {
                return std::nullopt;
            }

            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type, *boolean);
        }

        const auto& left_value = *left;
        const auto& right_value = *right;
        if (node.op == Operator::Equals || node.op == Operator::NotEquals) {
            const auto equal = left_value == right_value;
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type,
                                    node.op == Operator::Equals ? equal : !equal);
        }

        return ConstantMath::binary(output(), node.op, left_value, right_value, node.type, node.span);
    }

    std::optional<CompileTimeValue> visit(UnaryExpression& node) override {
        using Operator = UnaryExpression::Operator::Type;

        if (node.op == Operator::Minus) {
            if (auto* literal = node.operand->as<NumberLiteral>(); literal != nullptr) {
                return integer_literal(*literal, true);
            }
        }

        auto value = evaluate_value(*node.operand);
        if (!value.has_value()) {
            return std::nullopt;
        }

        switch (node.op) {
        case Operator::Plus: {
            const auto& resolved = *value;
            if (resolved.is_integer()) {
                return resolved;
            }
            break;
        }
        case Operator::Minus: {
            const auto& resolved = *value;
            if (resolved.is_integer()) {
                const auto numeric =
                    resolved.kind == CompileTimeValue::Kind::Type::SignedInteger
                        ? static_cast<__int128>(std::get<std::int64_t>(resolved.payload))
                        : static_cast<__int128>(std::get<std::uint64_t>(resolved.payload));
                return ConstantMath::integer(output(), -numeric, node.type,
                                             node.span, IntegerUse::Type::Arithmetic);
            }
            break;
        }
        case Operator::Not: {
            auto boolean = read_boolean(*value, node.span);
            if (boolean.has_value()) {
                return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type,
                                        !*boolean);
            }
            break;
        }
        case Operator::Dereference:
        case Operator::AddressOf:
        case Operator::AddressOfMut:
            break;
        }

        output().add_error(node.span, "unsupported unary operator in constant expression");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(CoerceExpression& node) override {
        auto value = evaluate_value(*node.value);
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (node.value->as<NullLiteral>() != nullptr) {
            return CompileTimeValue(CompileTimeValue::Kind::Type::Null, node.type);
        }

        if (node.type != nullptr && node.type->is_scalar()) {
            return ConstantMath::cast(output(), *value, node.type, node.span);
        }

        value->type = node.type;
        return value;
    }

    std::optional<CompileTimeValue> visit(CallExpression& node) override {
        output().add_error(node.span, "function calls are not allowed in constant expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BuiltinCall& node) override {
        if (node.name == "sizeof" && node.type_argument != nullptr) {
            const auto* type = node.type_argument->type;
            if (const auto* named = type != nullptr ? type->as<NamedType>() : nullptr;
                named != nullptr && environment != nullptr) {
                const auto* binding = named->declaration != nullptr
                                          ? environment->lookup(named->declaration)
                                          : environment->lookup(named->name);
                if (const auto* type_binding =
                        binding != nullptr ? std::get_if<TypeBinding>(binding) : nullptr;
                    type_binding != nullptr) {
                    type = type_binding->type;
                }
            }
            const auto layout = TypeLayout().resolve(type);
            if (layout.state != TypeLayout::Result::State::Concrete) {
                if (layout.state == TypeLayout::Result::State::Dependent &&
                    policy == EvaluationPolicy::Type::Dependent) {
                    dependent = true;
                    return std::nullopt;
                }
                return integer_error(node.span, "sizeof requires a concrete sized type");
            }

            return ConstantMath::integer(
                output(), static_cast<__int128>(layout.size), node.type, node.span);
        }

        if (node.name == "length" && node.arguments.size() == 1) {
            const auto* resolved = node.arguments[0]->type;
            if (const auto* identifier = node.arguments[0]->as<IdentifierExpression>();
                identifier != nullptr && env != nullptr && env->current_scope != nullptr) {
                const auto* symbol = env->current_scope->lookup_var(identifier->name);
                if (symbol != nullptr) {
                    resolved = symbol->type;
                }
            }
            const auto* array_type = resolved != nullptr ? resolved->as<ArrayType>() : nullptr;
            if (array_type != nullptr &&
                std::holds_alternative<ConcreteArrayExtent>(array_type->extent)) {
                return ConstantMath::integer(
                    output(),
                    static_cast<__int128>(
                        std::get<ConcreteArrayExtent>(array_type->extent).value),
                    node.type, node.span);
            }

            if (const auto* extent = array_type != nullptr
                                            ? std::get_if<DependentArrayExtent>(&array_type->extent)
                                            : nullptr;
                extent != nullptr && extent->expression != nullptr) {
                auto* expression = static_cast<Expression*>(extent->expression);
                const auto result =
                    Evaluator(output(), env, environment, policy).evaluate(*expression);
                const auto& length = result.value;
                const auto size =
                    length.has_value() ? length->try_as_unsigned_integer() : std::nullopt;
                if (size.has_value()) {
                    return ConstantMath::integer(
                        output(), static_cast<__int128>(*size), node.type, node.span);
                }

                dependent = result.state == EvaluationResult::State::Dependent;

                return std::nullopt;
            }

            if (array_type != nullptr &&
                std::holds_alternative<DependentArrayExtent>(array_type->extent) &&
                policy == EvaluationPolicy::Type::Dependent) {
                dependent = true;
                return std::nullopt;
            }
        }

        output().add_error(node.span, "unsupported builtin in constant expression");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(AssemblyExpression& node) override {
        output().add_error(node.span, "asm not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(IndexExpression& node) override {
        output().add_error(node.span, "indexing not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(MemberExpression& node) override {
        if (node.member == "value" && node.value->type != nullptr &&
            node.value->type->is<EnumType>()) {
            if (const auto* qualified = node.value->as<QualifiedAccessExpression>();
                qualified != nullptr && qualified->enum_value.has_value()) {
                return qualified->enum_value;
            }
        }

        output().add_error(node.span, "member access not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(QualifiedAccessExpression& node) override {
        output().add_error(node.span, "qualified access not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(NewExpression& node) override {
        output().add_error(node.span, "new not supported in const expressions");

        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(DeleteExpression& node) override {
        output().add_error(node.span, "delete not supported in const expressions");

        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(AssignExpression& node) override {
        output().add_error(node.span, "assignment not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(StructLiteralExpression& node) override {
        const auto* type = node.type;
        const auto* structure = type != nullptr ? type->as<StructType>() : nullptr;
        if (structure == nullptr) {
            return integer_error(node.span, "constant aggregate requires a resolved struct type");
        }

        std::vector<CompileTimeValue> fields;
        fields.reserve(structure->fields.size());

        for (const auto& field : structure->fields) {
            Expression* initializer = nullptr;
            for (auto* candidate : node.fields) {
                if (candidate->name == field.name) {
                    initializer = candidate->value;
                    break;
                }
            }

            if (initializer == nullptr) {
                return integer_error(node.span,
                                     "missing constant field initializer for '" + field.name + "'");
            }

            auto field_value = evaluate_value(*initializer);
            if (!field_value.has_value()) {
                return std::nullopt;
            }

            fields.push_back(std::move(*field_value));
        }

        return CompileTimeValue(CompileTimeValue::Kind::Type::Struct, node.type, std::move(fields));
    }

    std::optional<CompileTimeValue> visit(EnumVariantExpression& node) override {
        output().add_error(node.span, "enum variants not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(IfExpression& node) override {
        output().add_error(node.span, "if expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(WhenExpression& node) override {
        output().add_error(node.span, "when expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(ClosureExpression& node) override {
        output().add_error(node.span, "closures not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BlockExpression& node) override {
        output().add_error(node.span, "block expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(ArrayLiteralExpression& node) override {
        std::vector<CompileTimeValue> elements;
        elements.reserve(node.elements.size());

        for (auto* element : node.elements) {
            auto element_value = evaluate_value(*element);
            if (!element_value.has_value()) {
                return std::nullopt;
            }

            elements.push_back(std::move(*element_value));
        }

        return CompileTimeValue(CompileTimeValue::Kind::Type::Array, node.type,
                                std::move(elements));
    }

    std::optional<CompileTimeValue> visit(TypeExpression& node) override {
        output().add_error(node.span, "type expressions not supported in const expressions");
        return std::nullopt;
    }

};
