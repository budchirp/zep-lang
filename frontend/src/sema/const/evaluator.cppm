module;

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module zep.frontend.sema.constant.evaluator;

import zep.common.source.span;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.const_size;

export class Evaluator : public Visitor<std::optional<CompileTimeValue>> {
  private:
    Diagnostics& diagnostics;
    Env* env;
    const CompileTimeEnvironment* environment;
    bool allow_dependent;
    bool cache_results = true;

    std::optional<CompileTimeValue> integer_error(Span span, const std::string& message) {
        diagnostics.add_error(span, message);
        return std::nullopt;
    }

    std::optional<CompileTimeValue> checked_integer(__int128 value, const Type* type, Span span,
                                                    bool arithmetic = false) {
        return CompileTimeValue::checked_integer(diagnostics, value, type, span, arithmetic);
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
        return checked_integer(negative ? -value : value, type, node.span);
    }

    template <typename Number>
    std::optional<CompileTimeValue> integer_binary(BinaryExpression& node, Number left,
                                                   Number right) {
        return CompileTimeValue::evaluate_binary(diagnostics,
                                                 static_cast<BinaryOperator::Type>(node.op), left,
                                                 right, node.type, node.span);
    }

    std::optional<bool> read_boolean(CompileTimeValue value, Span span) {
        if (value.kind != CompileTimeValue::Kind::Type::Boolean) {
            diagnostics.add_error(span, "constant expression requires a concrete boolean value");
            return std::nullopt;
        }

        return std::get<bool>(value.payload);
    }

    std::optional<CompileTimeValue> cast_value(CompileTimeValue value, const Type* target_type,
                                               Span span) {
        return value.cast_to(diagnostics, target_type, span);
    }

  public:
    Evaluator(Diagnostics& diagnostics, Env* env = nullptr,
              const CompileTimeEnvironment* environment = nullptr, bool allow_dependent = false)
        : diagnostics(diagnostics), env(env), environment(environment),
          allow_dependent(allow_dependent) {}

    std::optional<CompileTimeValue> evaluate(Expression& node) {
        if (!cache_results) {
            return visit_expression(node);
        }

        if (node.compile_time_value.has_value()) {
            return node.compile_time_value;
        }

        auto result = visit_expression(node);
        node.compile_time_value = result;
        return result;
    }

    std::optional<CompileTimeValue> evaluate_uncached(Expression& node) {
        const auto previous = cache_results;
        cache_results = false;
        auto result = evaluate(node);
        cache_results = previous;
        return result;
    }

    std::optional<CompileTimeValue> visit(NumberLiteral& node) override {
        return integer_literal(node);
    }

    std::optional<CompileTimeValue> visit(FloatLiteral& node) override {
        diagnostics.add_error(node.span, "float literals not supported in const expressions");
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
                if (allow_dependent) {
                    return std::nullopt;
                }
                return integer_error(node.span,
                                     "constant expression requires a concrete generic argument '" +
                                         node.name + "'");
            }
        }

        if (node.generic_declaration != nullptr && allow_dependent) {
            return std::nullopt;
        }

        diagnostics.add_error(node.span,
                              "constant expression cannot use runtime value '" + node.name + "'");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BinaryExpression& node) override {
        if (node.op == BinaryExpression::Operator::Type::As) {
            auto value = evaluate(*node.left);
            if (!value.has_value()) {
                return std::nullopt;
            }
            return cast_value(*value, node.right->type, node.span);
        }

        auto left = evaluate(*node.left);
        if (!left.has_value()) {
            return std::nullopt;
        }

        using Op = BinaryExpression::Operator::Type;
        if (node.op == Op::And || node.op == Op::Or) {
            auto boolean = read_boolean(*left, node.left->span);
            if (!boolean.has_value()) {
                return std::nullopt;
            }

            if ((node.op == Op::And && !*boolean) || (node.op == Op::Or && *boolean)) {
                return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type, *boolean);
            }
        }

        auto right = evaluate(*node.right);
        if (!right.has_value()) {
            return std::nullopt;
        }

        if (node.op == Op::And || node.op == Op::Or) {
            auto boolean = read_boolean(*right, node.right->span);
            if (!boolean.has_value()) {
                return std::nullopt;
            }

            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type, *boolean);
        }

        const auto& left_value = *left;
        const auto& right_value = *right;
        if (node.op == Op::Equals || node.op == Op::NotEquals) {
            const auto equal = left_value == right_value;
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type,
                                    node.op == Op::Equals ? equal : !equal);
        }

        if (left_value.kind == CompileTimeValue::Kind::Type::UnsignedInteger &&
            right_value.kind == CompileTimeValue::Kind::Type::UnsignedInteger) {
            return integer_binary(node, std::get<std::uint64_t>(left_value.payload),
                                  std::get<std::uint64_t>(right_value.payload));
        }

        if (left_value.kind == CompileTimeValue::Kind::Type::SignedInteger &&
            right_value.kind == CompileTimeValue::Kind::Type::SignedInteger) {
            return integer_binary(node, std::get<std::int64_t>(left_value.payload),
                                  std::get<std::int64_t>(right_value.payload));
        }

        return integer_error(node.span, "constant expression requires matching integer operands");
    }

    std::optional<CompileTimeValue> visit(UnaryExpression& node) override {
        using Op = UnaryExpression::Operator::Type;
        if (node.op == Op::Minus) {
            if (auto* literal = node.operand->as<NumberLiteral>(); literal != nullptr) {
                return integer_literal(*literal, true);
            }
        }

        auto value = evaluate(*node.operand);
        if (!value.has_value()) {
            return std::nullopt;
        }

        switch (node.op) {
        case Op::Plus: {
            const auto& resolved = *value;
            if (resolved.is_integer()) {
                return resolved;
            }
            break;
        }
        case Op::Minus: {
            const auto& resolved = *value;
            if (resolved.is_integer()) {
                const auto numeric =
                    resolved.kind == CompileTimeValue::Kind::Type::SignedInteger
                        ? static_cast<__int128>(std::get<std::int64_t>(resolved.payload))
                        : static_cast<__int128>(std::get<std::uint64_t>(resolved.payload));
                return checked_integer(-numeric, node.type, node.span, true);
            }
            break;
        }
        case Op::Not: {
            auto boolean = read_boolean(*value, node.span);
            if (boolean.has_value()) {
                return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, node.type,
                                        !*boolean);
            }
            break;
        }
        case Op::Dereference:
        case Op::AddressOf:
        case Op::AddressOfMut:
            break;
        }

        diagnostics.add_error(node.span, "unsupported unary operator in constant expression");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(CoerceExpression& node) override {
        auto value = evaluate(*node.value);
        if (!value.has_value()) {
            return std::nullopt;
        }

        if (node.value->as<NullLiteral>() != nullptr) {
            return CompileTimeValue(CompileTimeValue::Kind::Type::Null, node.type);
        }

        if (node.type != nullptr && node.type->is_scalar()) {
            return cast_value(*value, node.type, node.span);
        }

        value->type = node.type;
        return value;
    }

    std::optional<CompileTimeValue> visit(CallExpression& node) override {
        diagnostics.add_error(node.span, "function calls are not allowed in constant expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BuiltinCall& node) override {
        if (node.name == "sizeof" && node.type_argument != nullptr) {
            const auto* type = node.type_argument->type;
            if (const auto* named = type != nullptr ? type->as<NamedType>() : nullptr;
                named != nullptr && environment != nullptr) {
                const auto* binding = environment->lookup_type(named->name, named->declaration);
                if (binding != nullptr) {
                    type = binding;
                }
            }
            auto size = compile_time_size_of(type);
            if (!size.has_value()) {
                if (allow_dependent) {
                    return std::nullopt;
                }
                return integer_error(node.span, "sizeof requires a concrete sized type");
            }

            return checked_integer(static_cast<__int128>(*size), node.type, node.span);
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
                return checked_integer(
                    static_cast<__int128>(std::get<ConcreteArrayExtent>(array_type->extent).value),
                    node.type, node.span);
            }

            if (const auto* dependent = array_type != nullptr
                                            ? std::get_if<DependentArrayExtent>(&array_type->extent)
                                            : nullptr;
                dependent != nullptr && dependent->expression != nullptr) {
                auto* expression =
                    const_cast<Expression*>(static_cast<const Expression*>(dependent->expression));
                const auto length = evaluate_uncached(*expression);
                const auto size =
                    length.has_value() ? length->try_as_unsigned_integer() : std::nullopt;
                if (size.has_value()) {
                    return checked_integer(static_cast<__int128>(*size), node.type, node.span);
                }
            }

            if (allow_dependent) {
                return std::nullopt;
            }
        }

        if (node.name == "asm") {
            diagnostics.add_error(node.span, "asm not supported in const expressions");
            return std::nullopt;
        }

        diagnostics.add_error(node.span, "unsupported builtin in constant expression");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(IndexExpression& node) override {
        diagnostics.add_error(node.span, "indexing not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(MemberExpression& node) override {
        diagnostics.add_error(node.span, "member access not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(QualifiedAccessExpression& node) override {
        diagnostics.add_error(node.span, "qualified access not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(AssignExpression& node) override {
        diagnostics.add_error(node.span, "assignment not supported in const expressions");
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

            auto field_value = evaluate(*initializer);
            if (!field_value.has_value()) {
                return std::nullopt;
            }

            fields.push_back(std::move(*field_value));
        }

        return CompileTimeValue(CompileTimeValue::Kind::Type::Struct, node.type, std::move(fields));
    }

    std::optional<CompileTimeValue> visit(EnumVariantExpression& node) override {
        diagnostics.add_error(node.span, "enum variants not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(IfExpression& node) override {
        diagnostics.add_error(node.span, "if expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(WhenExpression& node) override {
        diagnostics.add_error(node.span, "when expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(ClosureExpression& node) override {
        diagnostics.add_error(node.span, "closures not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(BlockExpression& node) override {
        diagnostics.add_error(node.span, "block expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit(ArrayLiteralExpression& node) override {
        std::vector<CompileTimeValue> elements;
        elements.reserve(node.elements.size());

        for (auto* element : node.elements) {
            auto element_value = evaluate(*element);
            if (!element_value.has_value()) {
                return std::nullopt;
            }

            elements.push_back(std::move(*element_value));
        }

        return CompileTimeValue(CompileTimeValue::Kind::Type::Array, node.type,
                                std::move(elements));
    }

    std::optional<CompileTimeValue> visit(TypeExpression& node) override {
        diagnostics.add_error(node.span, "type expressions not supported in const expressions");
        return std::nullopt;
    }

    std::optional<CompileTimeValue> visit([[maybe_unused]] BlockStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] ExpressionStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] ReturnStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] WhileStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] ForStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] DeferStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] InterfaceDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] StructDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] EnumDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] VarDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] FunctionDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue>
    visit([[maybe_unused]] ExternFunctionDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] ExternVarDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] ImportStatement& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] TypeAliasDeclaration& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] Attribute& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] GenericParameter& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] GenericArgument& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] Parameter& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] Argument& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] FunctionPrototype& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] Field& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] EnumVariant& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] StructLiteralField& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] WhenPatternField& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] WhenPattern& node) override {
        return std::nullopt;
    }
    std::optional<CompileTimeValue> visit([[maybe_unused]] WhenArm& node) override {
        return std::nullopt;
    }
};
