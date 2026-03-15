module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.checker.type_checker;

import zep.common.position;
import zep.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.checker.diagnostic;
import zep.checker.type_context;
import zep.checker.context;
import zep.sema.env;
import zep.sema.symbol;
import zep.sema.kinds;

export class TypeChecker : public Visitor<void> {
  private:
    Context& context;
    TypeContext type_context;

    std::shared_ptr<FunctionType> current_function_type;

  public:
    explicit TypeChecker(Context& context) : context(context), type_context(context.env) {}

    void visit_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>()) { visit(*node); return; }
        if (auto* node = expression.as<FloatLiteral>()) { visit(*node); return; }
        if (auto* node = expression.as<StringLiteral>()) { visit(*node); return; }
        if (auto* node = expression.as<BooleanLiteral>()) { visit(*node); return; }
        if (auto* node = expression.as<IdentifierExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<BinaryExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<UnaryExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<CallExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<IndexExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<MemberExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<AssignExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<StructLiteralExpression>()) { visit(*node); return; }
        if (auto* node = expression.as<IfExpression>()) { visit(*node); return; }
    }

    void visit_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>()) { visit(*node); return; }
        if (auto* node = statement.as<ExpressionStatement>()) { visit(*node); return; }
        if (auto* node = statement.as<ReturnStatement>()) { visit(*node); return; }
        if (auto* node = statement.as<StructDeclaration>()) { visit(*node); return; }
        if (auto* node = statement.as<VarDeclaration>()) { visit(*node); return; }
        if (auto* node = statement.as<FunctionDeclaration>()) { visit(*node); return; }
        if (auto* node = statement.as<ExternFunctionDeclaration>()) { visit(*node); return; }
        if (auto* node = statement.as<ExternVarDeclaration>()) { visit(*node); return; }
        if (auto* node = statement.as<ImportStatement>()) { visit(*node); return; }
    }

    bool is_mutable(Expression& expression) {
        if (auto* identifier = expression.as<IdentifierExpression>()) {
            auto* symbol = context.env.lookup(identifier->name);
            if (auto* var = symbol ? symbol->as<VarSymbol>() : nullptr) {
                return var->storage_kind == StorageKind::VarMut;
            }
            return false;
        }

        if (auto* unary = expression.as<UnaryExpression>()) {
            if (unary->op == UnaryExpression::UnaryOperator::Dereference) {
                auto operand_type = unary->operand->get_type();
                auto* ptr = operand_type ? operand_type->as<PointerType>() : nullptr;
                return ptr && ptr->is_mutable;
            }
            return false;
        }

        if (auto* member = expression.as<MemberExpression>()) {
            return is_mutable(*member->value);
        }

        if (auto* index = expression.as<IndexExpression>()) {
            return is_mutable(*index->value);
        }

        return false;
    }

    std::vector<GenericParameterType> build_generic_parameter_types(
        std::vector<std::unique_ptr<GenericParameter>>& generic_parameters) {
        std::vector<GenericParameterType> result;
        result.reserve(generic_parameters.size());

        for (auto& generic_parameter : generic_parameters) {
            std::shared_ptr<Type> constraint;
            if (generic_parameter->constraint) {
                visit(*generic_parameter->constraint);
                constraint = type_context.resolve_type(generic_parameter->constraint->get_type());
            }
            result.emplace_back(generic_parameter->name, std::move(constraint));
        }

        return result;
    }

    std::unordered_map<std::string, std::shared_ptr<Type>>
    resolve_generic_arguments(std::vector<std::unique_ptr<GenericArgument>>& generic_arguments,
                              const std::vector<GenericParameterType>& generic_parameters,
                              const Position& position) {
        std::unordered_map<std::string, std::shared_ptr<Type>> substitutions;

        if (generic_arguments.size() != generic_parameters.size()) {
            context.diagnostics.add_error(position, "expected " +
                                                        std::to_string(generic_parameters.size()) +
                                                        " generic argument(s), got " +
                                                        std::to_string(generic_arguments.size()));
            return substitutions;
        }

        for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
            visit(*generic_arguments[i]);
            auto arg_type = type_context.resolve_type(generic_arguments[i]->type->get_type());
            auto& generic_parameter = generic_parameters[i];

            if (generic_parameter.constraint && arg_type) {
                if (!type_context.types_compatible(arg_type, generic_parameter.constraint)) {
                    context.diagnostics.add_error(generic_arguments[i]->position,
                                                  "generic argument '" + arg_type->to_string() +
                                                      "' does not satisfy constraint '" +
                                                      generic_parameter.constraint->to_string() +
                                                      "'");
                }
            }

            substitutions[generic_parameter.name] = arg_type;
        }

        return substitutions;
    }

    std::shared_ptr<StructType> build_struct_type(StructDeclaration& node) {
        auto generic_parameter_types = build_generic_parameter_types(node.generic_parameters);

        std::vector<StructFieldType> field_types;
        field_types.reserve(node.fields.size());
        for (auto& field : node.fields) {
            visit(*field);
            field_types.emplace_back(field->name,
                                     type_context.resolve_type(field->type->get_type()));
        }

        return std::make_shared<StructType>(node.name, std::move(generic_parameter_types),
                                            std::move(field_types));
    }

    std::shared_ptr<FunctionType> build_function_type(FunctionPrototype& prototype) {
        auto generic_parameter_types = build_generic_parameter_types(prototype.generic_parameters);

        std::vector<ParameterType> parameter_types;
        bool is_variadic = false;
        parameter_types.reserve(prototype.parameters.size());
        for (auto& parameter : prototype.parameters) {
            visit(*parameter);
            auto parameter_type = type_context.resolve_type(parameter->type->get_type());
            parameter_types.emplace_back(parameter->name, parameter_type);
            if (parameter->is_variadic) {
                is_variadic = true;
            }
        }

        visit(*prototype.return_type);
        auto return_type = type_context.resolve_type(prototype.return_type->get_type());

        return std::make_shared<FunctionType>(return_type, std::move(parameter_types),
                                              std::move(generic_parameter_types), is_variadic);
    }

    void register_declarations(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* node = statement->as<StructDeclaration>()) {
                context.env.define_type(node->name, build_struct_type(*node));
            } else if (auto* node = statement->as<FunctionDeclaration>()) {
                auto& proto = *node->prototype;
                context.env.define(proto.name, std::make_unique<FunctionSymbol>(
                                                   proto.name, node->position, node->visibility,
                                                   build_function_type(proto)));
            } else if (auto* node = statement->as<ExternFunctionDeclaration>()) {
                auto& proto = *node->prototype;
                context.env.define(proto.name, std::make_unique<FunctionSymbol>(
                                                   proto.name, node->position, node->visibility,
                                                   build_function_type(proto)));
            } else if (auto* node = statement->as<ExternVarDeclaration>()) {
                if (node->type) visit(*node->type);
                context.env.define(node->name, std::make_unique<VarSymbol>(
                                                   node->name, node->position, node->visibility,
                                                   StorageKind::Var,
                                                   type_context.resolve_type(node->type->get_type())));
            }
        }
    }

  public:
    void check(Program& program) {
        context.env.push_scope();

        register_declarations(program);

        for (auto& statement : program.statements) {
            visit_statement(*statement);
        }

        context.env.pop_scope();
    }

    void visit(TypeExpression& node) override {
        node.set_type(type_context.resolve_type(node.type));
    }

    void visit(GenericParameter& node) override {
        if (node.constraint) {
            visit(*node.constraint);
        }
    }

    void visit(GenericArgument& node) override {
        if (node.type) {
            visit(*node.type);
        }
    }

    void visit(Parameter& node) override {
        if (node.type) {
            visit(*node.type);
        }
    }

    void visit(Argument& node) override { visit_expression(*node.value); }

    void visit(FunctionPrototype& node) override {
        for (auto& generic_parameter : node.generic_parameters) {
            visit(*generic_parameter);
        }
        for (auto& parameter : node.parameters) {
            visit(*parameter);
        }
        if (node.return_type) {
            visit(*node.return_type);
        }
    }

    void visit(StructField& node) override {
        if (node.type) {
            visit(*node.type);
        }
    }

    void visit(StructLiteralField& node) override { visit_expression(*node.value); }

    void visit(NumberLiteral& node) override {
        node.set_type(std::make_shared<IntegerType>(false, 32));
    }

    void visit(FloatLiteral& node) override { node.set_type(std::make_shared<FloatType>(32)); }

    void visit(StringLiteral& node) override { node.set_type(std::make_shared<StringType>()); }

    void visit(BooleanLiteral& node) override { node.set_type(std::make_shared<BooleanType>()); }

    void visit(IdentifierExpression& node) override {
        auto* symbol = context.env.lookup(node.name);
        if (!symbol) {
            context.diagnostics.add_error(node.position, "use of undeclared identifier '" + node.name + "'");
            return;
        }
        node.set_type(symbol->type);
    }

    void visit(BinaryExpression& node) override {
        using Op = BinaryExpression::BinaryOperator;

        if (node.op == Op::As || node.op == Op::Is) {
            visit_expression(*node.left);

            auto target_type = node.right->get_type();

            if (target_type == nullptr) {
                if (auto* identifier = node.right->as<IdentifierExpression>()) {
                    target_type = type_context.resolve_type_name(identifier->name);
                } else if (auto* unary = node.right->as<UnaryExpression>()) {
                    if (unary->op == UnaryExpression::UnaryOperator::Dereference) {
                        if (auto* inner = unary->operand->as<IdentifierExpression>()) {
                            auto element = type_context.resolve_type_name(inner->name);
                            if (element != nullptr) {
                                target_type = std::make_shared<PointerType>(std::move(element));
                            }
                        }
                    }
                }
            }

            if (target_type == nullptr) {
                context.diagnostics.add_error(
                    node.position, node.op == Op::As ? "invalid type in 'as' cast"
                                                           : "invalid type in 'is' expression");
                return;
            }

            auto resolved = type_context.resolve_type(target_type);
            node.right->set_type(resolved);

            if (node.op == Op::As) {
                node.set_type(resolved);
            } else {
                node.set_type(std::make_shared<BooleanType>());
            }

            return;
        }

        visit_expression(*node.left);
        visit_expression(*node.right);

        auto left_type = type_context.resolve_type(node.left->get_type());
        auto right_type = type_context.resolve_type(node.right->get_type());

        if (!left_type || !right_type) {
            return;
        }

        switch (node.op) {
        case Op::Plus:
        case Op::Minus:
        case Op::Asterisk:
        case Op::Divide:
        case Op::Modulo: {
            if (!left_type->is_numeric()) {
                context.diagnostics.add_error(
                    node.left->position,
                    "left operand of arithmetic operator must be numeric, got '" +
                        left_type->to_string() + "'");
                return;
            }
            if (!right_type->is_numeric()) {
                context.diagnostics.add_error(
                    node.right->position,
                    "right operand of arithmetic operator must be numeric, got '" +
                        right_type->to_string() + "'");
                return;
            }
            if (!type_context.types_compatible(left_type, right_type)) {
                context.diagnostics.add_error(
                    node.position, "type mismatch in arithmetic: '" + left_type->to_string() +
                                             "' and '" + right_type->to_string() + "'");
                return;
            }
            node.set_type(left_type);
            break;
        }
        case Op::Equals:
        case Op::NotEquals:
        case Op::LessThan:
        case Op::GreaterThan:
        case Op::LessEqual:
        case Op::GreaterEqual: {
            if (!type_context.types_compatible(left_type, right_type)) {
                context.diagnostics.add_error(
                    node.position, "type mismatch in comparison: '" + left_type->to_string() +
                                             "' and '" + right_type->to_string() + "'");
                return;
            }
            node.set_type(std::make_shared<BooleanType>());
            break;
        }
        case Op::And:
        case Op::Or: {
            if (left_type->kind != Type::Kind::Boolean) {
                context.diagnostics.add_error(
                    node.left->position,
                    "left operand of logical operator must be boolean, got '" +
                        left_type->to_string() + "'");
                return;
            }
            if (right_type->kind != Type::Kind::Boolean) {
                context.diagnostics.add_error(
                    node.right->position,
                    "right operand of logical operator must be boolean, got '" +
                        right_type->to_string() + "'");
                return;
            }
            node.set_type(std::make_shared<BooleanType>());
            break;
        }
        default:
            break;
        }
    }

    void visit(UnaryExpression& node) override {
        using Op = UnaryExpression::UnaryOperator;

        visit_expression(*node.operand);
        auto operand_type = type_context.resolve_type(node.operand->get_type());
        if (!operand_type) {
            return;
        }

        switch (node.op) {
        case Op::Plus:
        case Op::Minus: {
            if (!operand_type->is_numeric()) {
                context.diagnostics.add_error(node.position,
                                              "operand of unary +/- must be numeric, got '" +
                                                  operand_type->to_string() + "'");
                return;
            }
            node.set_type(operand_type);
            break;
        }
        case Op::Not: {
            if (operand_type->kind != Type::Kind::Boolean) {
                context.diagnostics.add_error(node.position,
                                              "operand of '!' must be boolean, got '" +
                                                  operand_type->to_string() + "'");
                return;
            }
            node.set_type(std::make_shared<BooleanType>());
            break;
        }
        case Op::Dereference: {
            if (auto* ptr = operand_type->as<PointerType>()) {
                node.set_type(ptr->element);
            } else {
                context.diagnostics.add_error(node.position,
                                              "cannot dereference non-pointer type '" + operand_type->to_string() + "'");
            }
            break;
        }
        case Op::AddressOf: {
            node.set_type(std::make_shared<PointerType>(operand_type));
            break;
        }
        }
    }

    void visit(CallExpression& node) override {
        visit_expression(*node.callee);
        auto callee_type = type_context.resolve_type(node.callee->get_type());

        if (!callee_type || callee_type->kind != Type::Kind::Function) {
            if (callee_type) {
                context.diagnostics.add_error(node.position,
                                              "cannot call non-function type '" +
                                                  callee_type->to_string() + "'");
            }
            return;
        }

        if (!callee_type->as<FunctionType>()) return;
        auto function_type = std::static_pointer_cast<FunctionType>(callee_type);

        auto call_substitutions = resolve_generic_arguments(
            node.generic_arguments, function_type->generic_parameters, node.position);

        for (auto& argument : node.arguments) {
            visit(*argument);
        }

        auto param_count = function_type->parameters.size();
        auto arg_count = node.arguments.size();

        if (function_type->variadic) {
            auto required_count = param_count > 0 ? param_count - 1 : 0;
            if (arg_count < required_count) {
                context.diagnostics.add_error(
                    node.position, "expected at least " + std::to_string(required_count) +
                                             " argument(s), got " + std::to_string(arg_count));
                return;
            }

            for (std::size_t i = 0; i < required_count && i < arg_count; ++i) {
                auto expected_type = function_type->parameters[i].type;
                if (!call_substitutions.empty()) {
                    expected_type =
                        TypeContext::substitute_generic_type(expected_type, call_substitutions);
                }

                auto actual_type = node.arguments[i]->value->get_type();
                if (!type_context.types_compatible(actual_type, expected_type)) {
                    context.diagnostics.add_error(node.arguments[i]->position,
                                                  "argument type mismatch: expected '" +
                                                      expected_type->to_string() + "', got '" +
                                                      actual_type->to_string() + "'");
                }
            }

            if (param_count > 0) {
                auto element_type = TypeContext::substitute_generic_type(
                    function_type->parameters.back().type, call_substitutions);
                if (element_type && element_type->kind == Type::Kind::Array) {
                    if (auto* arr = element_type->as<ArrayType>()) element_type = arr->element;
                }
                for (std::size_t i = required_count; i < arg_count; ++i) {
                    auto actual = node.arguments[i]->value->get_type();
                    if (element_type && actual && !type_context.types_compatible(actual, element_type)) {
                        context.diagnostics.add_error(node.arguments[i]->position,
                                                      "argument type mismatch: expected '" +
                                                          element_type->to_string() + "', got '" +
                                                          actual->to_string() + "'");
                    }
                }
            }
        } else {
            if (arg_count != param_count) {
                context.diagnostics.add_error(node.position,
                                              "expected " + std::to_string(param_count) +
                                                  " argument(s), got " + std::to_string(arg_count));
                return;
            }

            for (std::size_t i = 0; i < param_count; ++i) {
                auto expected_type = function_type->parameters[i].type;
                if (!call_substitutions.empty()) {
                    expected_type =
                        TypeContext::substitute_generic_type(expected_type, call_substitutions);
                }

                auto actual_type = node.arguments[i]->value->get_type();
                if (!type_context.types_compatible(actual_type, expected_type)) {
                    context.diagnostics.add_error(node.arguments[i]->position,
                                                  "argument type mismatch: expected '" +
                                                      expected_type->to_string() + "', got '" +
                                                      actual_type->to_string() + "'");
                }
            }
        }

        node.set_type(call_substitutions.empty()
                          ? function_type->return_type
                          : TypeContext::substitute_generic_type(function_type->return_type, call_substitutions));
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        auto value_type = type_context.resolve_type(node.value->get_type());
        if (!value_type) return;

        if (value_type->kind != Type::Kind::Array) {
            context.diagnostics.add_error(node.position,
                                          "cannot index non-array type '" + value_type->to_string() + "'");
            return;
        }

        auto index_type = type_context.resolve_type(node.index->get_type());
        if (index_type && index_type->kind != Type::Kind::Integer) {
            context.diagnostics.add_error(node.index->position,
                                          "array index must be an integer, got '" + index_type->to_string() + "'");
        }

        if (auto* array_type = value_type->as<ArrayType>()) {
            node.set_type(array_type->element);
        }
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);
        auto value_type = type_context.resolve_type(node.value->get_type());
        if (!value_type) return;

        auto* struct_type = value_type->as<StructType>();
        if (!struct_type) {
            context.diagnostics.add_error(node.position,
                                          "cannot access member of non-struct type '" + value_type->to_string() + "'");
            return;
        }

        for (const auto& field : struct_type->fields) {
            if (field.name == node.member) {
                node.set_type(field.type);
                return;
            }
        }
        context.diagnostics.add_error(node.position,
                                      "struct '" + struct_type->name + "' has no member '" + node.member + "'");
    }

    void visit(AssignExpression& node) override {
        visit_expression(*node.target);
        visit_expression(*node.value);

        if (!is_mutable(*node.target)) {
            context.diagnostics.add_error(node.target->position,
                                          "cannot assign to immutable target");
        }

        auto target_type = type_context.resolve_type(node.target->get_type());
        auto value_type = type_context.resolve_type(node.value->get_type());

        if (target_type && value_type && !type_context.types_compatible(target_type, value_type)) {
            context.diagnostics.add_error(node.position,
                                          "type mismatch in assignment: expected '" +
                                              target_type->to_string() + "', got '" +
                                              value_type->to_string() + "'");
        }

        node.set_type(target_type);
    }

    void visit(StructLiteralExpression& node) override {
        auto struct_type_ptr = context.env.lookup_type(node.name->name);
        if (!struct_type_ptr || struct_type_ptr->kind != Type::Kind::Struct) {
            context.diagnostics.add_error(node.position,
                                          "unknown struct type '" + node.name->name + "'");
            return;
        }
        auto* struct_type = struct_type_ptr->as<StructType>();
        if (!struct_type) return;

        auto struct_substitutions = resolve_generic_arguments(
            node.generic_arguments, struct_type->generic_parameters, node.position);

        std::unordered_map<std::string, bool> provided_fields;

        for (auto& literal_field : node.fields) {
            visit(*literal_field);

            if (provided_fields.contains(literal_field->name)) {
                context.diagnostics.add_error(literal_field->position,
                                              "duplicate field '" + literal_field->name + "'");
                continue;
            }
            provided_fields[literal_field->name] = true;

            bool found = false;
            for (const auto& declared_field : struct_type->fields) {
                if (declared_field.name == literal_field->name) {
                    found = true;
                    auto expected_type = declared_field.type;
                    if (!struct_substitutions.empty()) {
                        expected_type = TypeContext::substitute_generic_type(expected_type,
                                                                             struct_substitutions);
                    }

                    auto actual_type = literal_field->value->get_type();
                    if (!type_context.types_compatible(actual_type, expected_type)) {
                        context.diagnostics.add_error(literal_field->position,
                                                      "field '" + literal_field->name +
                                                          "' type mismatch: expected '" +
                                                          expected_type->to_string() + "', got '" +
                                                          actual_type->to_string() + "'");
                    }
                    break;
                }
            }

            if (!found) {
                context.diagnostics.add_error(literal_field->position,
                                              "struct '" + struct_type->name + "' has no field '" +
                                                  literal_field->name + "'");
            }
        }

        for (const auto& declared_field : struct_type->fields) {
            if (!provided_fields.contains(declared_field.name)) {
                context.diagnostics.add_error(
                    node.position, "missing field '" + declared_field.name + "' in struct '" +
                                             struct_type->name + "' literal");
            }
        }

        auto instantiated_struct_type =
            TypeContext::substitute_generic_type(struct_type_ptr, struct_substitutions);
        node.set_type(type_context.resolve_type(instantiated_struct_type));
    }

    void visit(BlockStatement& node) override {
        context.env.push_scope();

        for (auto& statement : node.statements) {
            visit_statement(*statement);
        }

        if (!node.statements.empty()) {
            if (auto* expr_stmt = node.statements.back()->as<ExpressionStatement>();
                expr_stmt && expr_stmt->expression) {
                node.set_type(expr_stmt->expression->get_type());
            }
        }
        if (!node.get_type()) node.set_type(std::make_shared<VoidType>());
        context.env.pop_scope();
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);
        node.set_type(node.expression->get_type());
    }

    void visit(IfExpression& node) override {
        visit_expression(*node.condition);

        auto condition_type = type_context.resolve_type(node.condition->get_type());
        if (condition_type && condition_type->kind != Type::Kind::Boolean) {
            context.diagnostics.add_error(node.condition->position,
                                          "if condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch) {
            visit_statement(*node.else_branch);

            auto then_type = node.then_branch->get_type();
            auto else_type = node.else_branch->get_type();

            if (then_type && else_type && !type_context.types_compatible(then_type, else_type)) {
                context.diagnostics.add_error(node.position,
                                              "if/else branches have different types: '" +
                                                  then_type->to_string() + "' and '" +
                                                  else_type->to_string() + "'");
            }

            node.set_type(then_type ? then_type : else_type);
        } else {
            node.set_type(std::make_shared<VoidType>());
        }
    }

    void visit(ReturnStatement& node) override {
        if (node.value) {
            visit_expression(*node.value);
        }

        if (!current_function_type) {
            return;
        }

        auto expected = type_context.resolve_type(current_function_type->return_type);
        if (!expected) {
            return;
        }

        if (node.value) {
            auto actual = type_context.resolve_type(node.value->get_type());
            if (actual && !type_context.types_compatible(actual, expected)) {
                context.diagnostics.add_error(node.position,
                                              "return type mismatch: expected '" +
                                                  expected->to_string() + "', got '" +
                                                  actual->to_string() + "'");
            }
        } else if (expected->kind != Type::Kind::Void) {
            context.diagnostics.add_error(node.position,
                                          "non-void function must return a value");
        }
    }

    void visit(StructDeclaration& node) override {
        if (context.env.lookup_type(node.name) != nullptr) {
            return;
        }

        context.env.define_type(node.name, build_struct_type(node));
    }

    void visit(VarDeclaration& node) override {
        if (node.type) visit(*node.type);
        if (node.initializer) visit_expression(*node.initializer);

        auto var_type = node.type ? type_context.resolve_type(node.type->get_type()) : nullptr;
        if (!var_type && node.initializer) var_type = type_context.resolve_type(node.initializer->get_type());

        if (node.type && node.initializer && var_type) {
            auto init_type = type_context.resolve_type(node.initializer->get_type());
            if (init_type && !type_context.types_compatible(var_type, init_type)) {
                context.diagnostics.add_error(node.position,
                                             "type mismatch in variable declaration: declared '" +
                                                 var_type->to_string() + "', initializer is '" +
                                                 init_type->to_string() + "'");
            }
        }

        if (!var_type) {
            context.diagnostics.add_error(node.position, "cannot determine type of variable '" + node.name + "'");
            var_type = std::make_shared<UnknownType>();
        }

        if (!context.env.define(node.name, std::make_unique<VarSymbol>(
                                               node.name, node.position, node.visibility,
                                               node.storage_kind, var_type))) {
            context.diagnostics.add_error(node.position, "redefinition of '" + node.name + "'");
        }
        node.set_type(var_type);
    }

    void visit(FunctionDeclaration& node) override {
        std::shared_ptr<FunctionType> function_type;
        if (auto* symbol = context.env.lookup(node.prototype->name);
            symbol && symbol->type && symbol->type->as<FunctionType>()) {
            function_type = std::static_pointer_cast<FunctionType>(symbol->type);
        }

        if (!function_type) {
            function_type = build_function_type(*node.prototype);
            context.env.define(node.prototype->name, std::make_unique<FunctionSymbol>(
                                                         node.prototype->name, node.position,
                                                         node.visibility, function_type));
        }

        auto previous_function_type = current_function_type;
        current_function_type = function_type;

        auto previous_substitutions = type_context.save_substitutions();
        for (const auto& generic_parameter : function_type->generic_parameters) {
            if (generic_parameter.constraint) {
                type_context.add_substitution(generic_parameter.name, generic_parameter.constraint);
            } else {
                type_context.add_substitution(generic_parameter.name, std::make_shared<AnyType>());
            }
        }

        context.env.push_scope();

        for (const auto& parameter : function_type->parameters) {
            auto param_type = parameter.type;
            if (type_context.has_substitutions()) {
                param_type = TypeContext::substitute_generic_type(param_type,
                                                                  type_context.get_substitutions());
            }
            context.env.define(parameter.name,
                               std::make_unique<VarSymbol>(parameter.name, node.position,
                                                           Visibility::Private, StorageKind::Var,
                                                           param_type));
        }

        visit(*node.body);

        context.env.pop_scope();

        type_context.restore_substitutions(std::move(previous_substitutions));
        current_function_type = previous_function_type;
    }

    void visit(ExternFunctionDeclaration& node) override {
        if (context.env.lookup(node.prototype->name) != nullptr) {
            return;
        }

        auto function_type = build_function_type(*node.prototype);
        context.env.define(node.prototype->name, std::make_unique<FunctionSymbol>(
                                                     node.prototype->name, node.position,
                                                     node.visibility, function_type));
    }

    void visit(ExternVarDeclaration& node) override {
        if (context.env.lookup(node.name) != nullptr) {
            return;
        }

        if (node.type) {
            visit(*node.type);
        }

        auto var_type = type_context.resolve_type(node.type->get_type());
        context.env.define(node.name, std::make_unique<VarSymbol>(node.name, node.position,
                                                                  node.visibility, StorageKind::Var,
                                                                  var_type));
    }

    void visit(ImportStatement& node [[maybe_unused]]) override {}
};
