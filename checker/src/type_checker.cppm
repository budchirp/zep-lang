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

    void check(Program& program) {
        register_declarations(program);

        for (auto& statement : program.statements) {
            visit_statement(*statement);
        }
    }

    void visit_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<FloatLiteral>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<StringLiteral>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<BooleanLiteral>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<IdentifierExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<BinaryExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<UnaryExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<CallExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<IndexExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<MemberExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<AssignExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<StructLiteralExpression>()) {
            visit(*node);
            return;
        }
        if (auto* node = expression.as<IfExpression>()) {
            visit(*node);
            return;
        }
    }

    void visit_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<ExpressionStatement>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<ReturnStatement>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<StructDeclaration>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<VarDeclaration>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<FunctionDeclaration>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<ExternFunctionDeclaration>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<ExternVarDeclaration>()) {
            visit(*node);
            return;
        }
        if (auto* node = statement.as<ImportStatement>()) {
            visit(*node);
            return;
        }
    }

    bool is_mutable(Expression& expression) {
        if (auto* node = expression.as<IdentifierExpression>()) {
            const auto* var = context.env.current_scope->lookup_var(node->name);
            if (var != nullptr) {
                return var->storage_kind == StorageKind::VarMut;
            }

            return false;
        }

        if (auto* node = expression.as<UnaryExpression>()) {
            if (node->op == UnaryExpression::UnaryOperator::Dereference) {
                auto type = node->operand->get_type();
                if (auto* pointer = type->as<PointerType>()) {
                    return pointer->is_mutable;
                }
            }

            return false;
        }

        if (auto* node = expression.as<MemberExpression>()) {
            return is_mutable(*node->value);
        }

        if (auto* node = expression.as<IndexExpression>()) {
            return is_mutable(*node->value);
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
                constraint = generic_parameter->constraint->get_type();
            }

            result.emplace_back(generic_parameter->name, std::move(constraint));
        }

        return result;
    }

    std::shared_ptr<FunctionType> find_function_overload(const std::string& name,
                                                         const FunctionType& signature) {
        for (const auto* sym : context.env.current_scope->lookup_function_overloads(name)) {
            auto* function_type = sym->type ? sym->type->as<FunctionType>() : nullptr;
            if (!function_type || function_type->parameters.size() != signature.parameters.size()) {
                continue;
            }

            bool match = true;
            for (std::size_t i = 0; i < function_type->parameters.size() && match; ++i) {
                if (!Type::compatible(function_type->parameters[i].type,
                                      signature.parameters[i].type)) {
                    match = false;
                }
            }

            if (match) {
                return std::static_pointer_cast<FunctionType>(sym->type);
            }
        }

        return nullptr;
    }

    std::shared_ptr<Type> select_overload(const std::string& name,
                                          const std::vector<const Symbol*>& overloads,
                                          CallExpression& node) {
        const Symbol* match = nullptr;
        int match_count = 0;

        for (const auto* sym : overloads) {
            auto* function_type = sym->type ? sym->type->as<FunctionType>() : nullptr;
            if (!function_type) {
                continue;
            }

            auto param_count = function_type->parameters.size();
            auto required =
                function_type->variadic && param_count > 0 ? param_count - 1 : param_count;

            if (function_type->generic_parameters.size() != node.generic_arguments.size()) {
                continue;
            }

            if (function_type->variadic ? node.arguments.size() < required
                                        : node.arguments.size() != param_count) {
                continue;
            }

            auto scope = type_context.scoped_substitutions();
            bool ok = true;

            for (std::size_t i = 0; i < node.generic_arguments.size() && ok; ++i) {
                visit(*node.generic_arguments[i]);
                auto arg_type = node.generic_arguments[i]->type->get_type();
                if (!arg_type) {
                    ok = false;
                    continue;
                }
                const auto& generic_parameter = function_type->generic_parameters[i];
                if (generic_parameter.constraint &&
                    !Type::compatible(arg_type, generic_parameter.constraint)) {
                    ok = false;
                    continue;
                }
                type_context.add_substitution(generic_parameter.name, arg_type);
            }

            for (std::size_t i = 0; i < required && ok; ++i) {
                auto expected = type_context.resolve_type(function_type->parameters[i].type);
                auto actual = node.arguments[i]->value->get_type();
                if (!actual || !Type::compatible(actual, expected)) {
                    ok = false;
                }
            }

            if (ok) {
                match = sym;
                ++match_count;
            }
        }

        if (match_count == 0) {
            context.diagnostics.add_error(node.position, "no matching overload for '" + name + "'");
            return nullptr;
        }

        if (match_count > 1) {
            context.diagnostics.add_error(node.position, "ambiguous call to '" + name + "'");
            return nullptr;
        }

        return match->type;
    }

    void resolve_generic_arguments(std::vector<std::unique_ptr<GenericArgument>>& generic_arguments,
                                   const std::vector<GenericParameterType>& generic_parameters,
                                   const Position& position) {
        if (generic_arguments.size() != generic_parameters.size()) {
            context.diagnostics.add_error(position, "expected " +
                                                        std::to_string(generic_parameters.size()) +
                                                        " generic argument(s), got " +
                                                        std::to_string(generic_arguments.size()));
            return;
        }

        for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
            visit(*generic_arguments[i]);
            auto argument_type = generic_arguments[i]->type->get_type();
            const auto& generic_parameter = generic_parameters[i];

            if (generic_parameter.constraint) {
                if (!Type::compatible(argument_type, generic_parameter.constraint)) {
                    context.diagnostics.add_error(
                        generic_arguments[i]->position,
                        "generic argument '" + argument_type->to_string() +
                            "' does not satisfy constraint '" +
                            generic_parameter.constraint->to_string() + "'");
                }
            }

            type_context.add_substitution(generic_parameter.name, argument_type);
        }
    }

    std::shared_ptr<StructType> build_struct_type(StructDeclaration& node) {
        auto generic_parameter_types = build_generic_parameter_types(node.generic_parameters);

        std::vector<StructFieldType> field_types;
        field_types.reserve(node.fields.size());

        for (auto& field : node.fields) {
            visit(*field);
            field_types.emplace_back(field->name, field->type->get_type());
        }

        return std::make_shared<StructType>(node.name, std::move(generic_parameter_types),
                                            std::move(field_types));
    }

    std::shared_ptr<FunctionType> build_function_type(FunctionPrototype& prototype) {
        auto generic_parameter_types = build_generic_parameter_types(prototype.generic_parameters);

        bool is_variadic = false;

        std::vector<ParameterType> parameter_types;
        parameter_types.reserve(prototype.parameters.size());

        for (auto& parameter : prototype.parameters) {
            visit(*parameter);
            parameter_types.emplace_back(parameter->name, parameter->type->get_type());

            if (parameter->is_variadic) {
                is_variadic = true;
            }
        }

        visit(*prototype.return_type);
        auto return_type = prototype.return_type->get_type();

        return std::make_shared<FunctionType>(return_type, std::move(parameter_types),
                                              std::move(generic_parameter_types), is_variadic);
    }

    void register_declarations(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* node = statement->as<StructDeclaration>()) {
                auto type = build_struct_type(*node);

                context.env.current_scope->define_type(node->name, std::move(type));
            } else if (auto* node = statement->as<VarDeclaration>()) {
                if (node->type) {
                    visit(*node->type);
                }

                auto type = node->type->get_type();

                context.env.current_scope->define_var(
                    node->name,
                    std::make_unique<VarSymbol>(node->name, node->position, node->visibility,
                                                node->storage_kind, std::move(type)));
            } else if (auto* node = statement->as<FunctionDeclaration>()) {
                auto& proto = *node->prototype;

                auto type = build_function_type(proto);

                if (!context.env.current_scope->define_function(
                        proto.name,
                        std::make_unique<FunctionSymbol>(proto.name, node->position,
                                                         node->visibility, std::move(type)))) {
                    context.diagnostics.add_error(node->position,
                                                  "redefinition of function '" + proto.name +
                                                      "' with same parameter types");
                }
            } else if (auto* node = statement->as<ExternFunctionDeclaration>()) {
                auto& proto = *node->prototype;

                auto type = build_function_type(proto);

                context.env.current_scope->define_function(
                    proto.name, std::make_unique<FunctionSymbol>(
                                    proto.name, node->position, node->visibility, std::move(type)));
            } else if (auto* node = statement->as<ExternVarDeclaration>()) {
                visit(*node->type);

                context.env.current_scope->define_var(
                    node->name,
                    std::make_unique<VarSymbol>(node->name, node->position, node->visibility,
                                                StorageKind::Var, node->type->get_type()));
            }
        }
    }

    void visit(TypeExpression& node) override {
        node.set_type(type_context.resolve_type(node.type));
    }

    void visit(GenericParameter& node) override {
        if (node.constraint) {
            visit(*node.constraint);
        }
    }

    void visit(GenericArgument& node) override { visit(*node.type); }

    void visit(Parameter& node) override { visit(*node.type); }

    void visit(Argument& node) override { visit_expression(*node.value); }

    void visit(FunctionPrototype& node) override {
        for (auto& generic_parameter : node.generic_parameters) {
            visit(*generic_parameter);
        }

        for (auto& parameter : node.parameters) {
            visit(*parameter);
        }

        visit(*node.return_type);
    }

    void visit(StructField& node) override { visit(*node.type); }

    void visit(StructLiteralField& node) override { visit_expression(*node.value); }

    void visit(NumberLiteral& node) override {
        node.set_type(std::make_shared<IntegerType>(false, 32));
    }

    void visit(FloatLiteral& node) override { node.set_type(std::make_shared<FloatType>(32)); }

    void visit(StringLiteral& node) override { node.set_type(std::make_shared<StringType>()); }

    void visit(BooleanLiteral& node) override { node.set_type(std::make_shared<BooleanType>()); }

    void visit(IdentifierExpression& node) override {
        const Symbol* symbol = context.env.current_scope->lookup_var(node.name);
        if (symbol == nullptr) {
            symbol = context.env.current_scope->lookup_function(node.name);
        }
        if (symbol != nullptr) {
            node.set_type(symbol->type);
            return;
        }

        auto type = context.env.current_scope->lookup_type(node.name);
        if (type != nullptr) {
            node.set_type(type);
            return;
        }

        context.diagnostics.add_error(node.position,
                                      "use of undeclared identifier '" + node.name + "'");
    }

    void visit(BinaryExpression& node) override {
        using Op = BinaryExpression::BinaryOperator;

        switch (node.op) {
        case Op::As:
        case Op::Is: {
            visit_expression(*node.left);

            auto type = node.right->get_type();

            if (node.op == Op::As) {
                node.set_type(type_context.resolve_type(type));
            } else {
                node.set_type(std::make_shared<BooleanType>());
            }
            break;
        }
        default: {
            visit_expression(*node.left);
            visit_expression(*node.right);

            auto left_type = node.left->get_type();
            auto right_type = node.right->get_type();

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

                if (!Type::compatible(left_type, right_type)) {
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
                if (!Type::compatible(left_type, right_type)) {
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
        }
    }

    void visit(UnaryExpression& node) override {
        using Op = UnaryExpression::UnaryOperator;

        visit_expression(*node.operand);

        auto operand_type = node.operand->get_type();
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
            if (auto* pointer = operand_type->as<PointerType>()) {
                node.set_type(pointer->element);
            } else {
                context.diagnostics.add_error(node.position,
                                              "cannot dereference non-pointer type '" +
                                                  operand_type->to_string() + "'");
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
        bool overloaded = false;

        if (auto* identifier = node.callee->as<IdentifierExpression>()) {
            auto overloads = context.env.current_scope->lookup_function_overloads(identifier->name);

            std::size_t function_count = 0;
            for (const auto* sym : overloads) {
                if (sym->kind == Symbol::Kind::Function) {
                    ++function_count;
                }
            }

            if (function_count > 1) {
                overloaded = true;
                for (auto& argument : node.arguments) {
                    visit(*argument);
                }

                std::vector<const Symbol*> symbol_overloads;
                for (const auto* sym : overloads) {
                    symbol_overloads.push_back(sym);
                }
                auto selected = select_overload(identifier->name, symbol_overloads, node);
                if (!selected) {
                    return;
                }

                node.callee->set_type(selected);
            }
        }

        if (!overloaded) {
            visit_expression(*node.callee);
        }

        auto callee_type = node.callee->get_type();
        if (!callee_type) {
            return;
        }

        auto* function_type = callee_type->as<FunctionType>();
        if (function_type == nullptr) {
            context.diagnostics.add_error(node.position, "cannot call non-function type '" +
                                                             callee_type->to_string() + "'");
            return;
        }

        auto scope = type_context.scoped_substitutions();
        resolve_generic_arguments(node.generic_arguments, function_type->generic_parameters,
                                  node.position);

        if (!overloaded) {
            for (auto& argument : node.arguments) {
                visit(*argument);
            }
        }

        auto parameter_count = function_type->parameters.size();
        auto argument_count = node.arguments.size();
        auto required =
            function_type->variadic && parameter_count > 0 ? parameter_count - 1 : parameter_count;

        if (function_type->variadic) {
            if (argument_count < required) {
                context.diagnostics.add_error(
                    node.position, "expected at least " + std::to_string(required) +
                                       " argument(s), got " + std::to_string(argument_count));
                return;
            }
        } else if (argument_count != parameter_count) {
            context.diagnostics.add_error(
                node.position, "expected " + std::to_string(parameter_count) +
                                   " argument(s), got " + std::to_string(argument_count));
            return;
        }

        for (std::size_t i = 0; i < required; ++i) {
            auto expected_type = type_context.resolve_type(function_type->parameters[i].type);
            auto actual_type = node.arguments[i]->value->get_type();
            if (!Type::compatible(actual_type, expected_type)) {
                context.diagnostics.add_error(node.arguments[i]->position,
                                              "argument type mismatch: expected '" +
                                                  expected_type->to_string() + "', got '" +
                                                  actual_type->to_string() + "'");
            }
        }

        if (function_type->variadic && parameter_count > 0) {
            auto element_type = type_context.resolve_type(function_type->parameters.back().type);
            if (!element_type) {
                return;
            }

            if (auto* arr = element_type->as<ArrayType>()) {
                element_type = arr->element;
            }

            for (std::size_t i = required; i < argument_count; ++i) {
                auto actual = node.arguments[i]->value->get_type();
                if (!actual) {
                    continue;
                }

                if (!Type::compatible(actual, element_type)) {
                    context.diagnostics.add_error(node.arguments[i]->position,
                                                  "argument type mismatch: expected '" +
                                                      element_type->to_string() + "', got '" +
                                                      actual->to_string() + "'");
                }
            }
        }

        node.set_type(type_context.resolve_type(function_type->return_type));
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        auto value_type = node.value->get_type();
        if (!value_type) {
            return;
        }

        auto* array_type = value_type->as<ArrayType>();
        if (array_type == nullptr) {
            context.diagnostics.add_error(node.position, "cannot index non-array type '" +
                                                             value_type->to_string() + "'");
            return;
        }

        auto index_type = node.index->get_type();
        if (!index_type) {
            return;
        }

        if (index_type->kind != Type::Kind::Integer) {
            context.diagnostics.add_error(node.index->position,
                                          "array index must be an integer, got '" +
                                              index_type->to_string() + "'");
        }

        node.set_type(array_type->element);
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        auto value_type = node.value->get_type();
        if (!value_type) {
            return;
        }

        auto* struct_type = value_type->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.position,
                                          "cannot access member of non-struct type '" +
                                              value_type->to_string() + "'");
            return;
        }

        for (const auto& field : struct_type->fields) {
            if (field.name == node.member) {
                node.set_type(field.type);
                return;
            }
        }

        context.diagnostics.add_error(node.position, "struct '" + struct_type->name +
                                                         "' has no member '" + node.member + "'");
    }

    void visit(AssignExpression& node) override {
        visit_expression(*node.target);
        visit_expression(*node.value);

        if (!is_mutable(*node.target)) {
            context.diagnostics.add_error(node.target->position,
                                          "cannot assign to immutable target");
        }

        auto target_type = node.target->get_type();
        auto value_type = node.value->get_type();

        if (!target_type || !value_type) {
            return;
        }

        if (!Type::compatible(target_type, value_type)) {
            context.diagnostics.add_error(node.position, "type mismatch in assignment: expected '" +
                                                             target_type->to_string() + "', got '" +
                                                             value_type->to_string() + "'");
        }

        node.set_type(target_type);
    }

    void visit(StructLiteralExpression& node) override {
        visit(*node.name);

        auto name_type = node.name->get_type();
        if (!name_type) {
            return;
        }

        auto* struct_type = name_type->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.position, "cannot construct non-struct type '" +
                                                             name_type->to_string() + "'");
            return;
        }

        auto scope = type_context.scoped_substitutions();
        resolve_generic_arguments(node.generic_arguments, struct_type->generic_parameters,
                                  node.position);

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
                    auto expected_type = type_context.resolve_type(declared_field.type);
                    auto actual_type = literal_field->value->get_type();
                    if (!Type::compatible(actual_type, expected_type)) {
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

        node.set_type(type_context.resolve_type(name_type));
    }

    void visit(BlockStatement& node) override {
        auto* scope = context.env.current_scope;
        context.env.push_scope(scope->name + ".block." + std::to_string(scope->child_count()));

        std::shared_ptr<Type> type;
        for (auto& statement : node.statements) {
            visit_statement(*statement);

            type = statement->get_type();
        }

        node.set_type(type);

        context.env.pop_scope();
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);

        node.set_type(node.expression->get_type());
    }

    void visit(IfExpression& node) override {
        visit_expression(*node.condition);

        auto condition_type = node.condition->get_type();
        if (!condition_type) {
            return;
        }

        if (condition_type->kind != Type::Kind::Boolean) {
            context.diagnostics.add_error(node.condition->position,
                                          "if condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch) {
            visit_statement(*node.else_branch);

            auto then_type = node.then_branch->get_type();
            auto else_type = node.else_branch->get_type();

            if (!then_type || !else_type) {
                return;
            }

            if (!Type::compatible(then_type, else_type)) {
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
        if (!current_function_type) {
            context.diagnostics.add_error(node.position, "return statement outside of function");
        }

        auto expected = type_context.resolve_type(current_function_type->return_type);
        if (!expected) {
            return;
        }

        if (node.value) {
            visit_expression(*node.value);

            auto type = node.value->get_type();
            if (!type) {
                return;
            }

            if (!Type::compatible(type, expected)) {
                context.diagnostics.add_error(
                    node.position, "return type mismatch: expected '" + expected->to_string() +
                                       "', got '" + type->to_string() + "'");
            }
        } else if (expected->kind != Type::Kind::Void) {
            context.diagnostics.add_error(node.position, "non-void function must return a value");
        }
    }

    void visit(StructDeclaration& node) override {
        if (context.env.current_scope->lookup_type(node.name) != nullptr) {
            return;
        }

        context.env.current_scope->define_type(node.name, build_struct_type(node));
    }

    void visit(VarDeclaration& node) override {
        if (node.type) {
            visit(*node.type);
        }
        if (node.initializer) {
            visit_expression(*node.initializer);
        }

        auto var_type = node.type ? node.type->get_type() : nullptr;
        if (!var_type && node.initializer) {
            var_type = node.initializer->get_type();
        }

        if (node.type && node.initializer && var_type) {
            auto init_type = node.initializer->get_type();
            if (init_type && !Type::compatible(var_type, init_type)) {
                context.diagnostics.add_error(node.position,
                                              "type mismatch in variable declaration: declared '" +
                                                  var_type->to_string() + "', initializer is '" +
                                                  init_type->to_string() + "'");
            }
        }

        if (!var_type) {
            context.diagnostics.add_error(node.position,
                                          "cannot determine type of variable '" + node.name + "'");
            var_type = std::make_shared<UnknownType>();
        }

        if (!context.env.current_scope->define_var(
                node.name, std::make_unique<VarSymbol>(node.name, node.position, node.visibility,
                                                       node.storage_kind, var_type))) {
            context.diagnostics.add_error(node.position, "redefinition of '" + node.name + "'");
        }
        node.set_type(var_type);
    }

    void visit(FunctionDeclaration& node) override {
        if (context.env.current_scope != context.env.global_scope.get()) {
            context.diagnostics.add_error(node.position,
                                          "functions can only be declared in global scope");
            return;
        }

        auto built_type = build_function_type(*node.prototype);

        auto function_type = find_function_overload(node.prototype->name, *built_type);
        if (!function_type) {
            function_type = built_type;
            context.env.current_scope->define_function(
                node.prototype->name,
                std::make_unique<FunctionSymbol>(node.prototype->name, node.position,
                                                 node.visibility, function_type));
        }

        auto previous_function_type = current_function_type;
        current_function_type = function_type;

        auto scope = type_context.scoped_substitutions();
        for (const auto& generic_parameter : function_type->generic_parameters) {
            if (generic_parameter.constraint) {
                type_context.add_substitution(generic_parameter.name, generic_parameter.constraint);
            } else {
                type_context.add_substitution(generic_parameter.name, std::make_shared<AnyType>());
            }
        }

        context.env.push_scope(context.env.current_scope->name + "." + node.prototype->name);

        for (const auto& parameter : function_type->parameters) {
            auto param_type = type_context.resolve_type(parameter.type);
            context.env.current_scope->define_var(
                parameter.name,
                std::make_unique<VarSymbol>(parameter.name, node.position, Visibility::Private,
                                            StorageKind::Var, param_type));
        }

        visit(*node.body);

        context.env.pop_scope();

        current_function_type = previous_function_type;
    }

    void visit(ExternFunctionDeclaration& node) override {
        if (context.env.current_scope->lookup_var(node.prototype->name) != nullptr ||
            context.env.current_scope->lookup_function(node.prototype->name) != nullptr) {
            return;
        }

        auto function_type = build_function_type(*node.prototype);
        context.env.current_scope->define_function(
            node.prototype->name,
            std::make_unique<FunctionSymbol>(node.prototype->name, node.position, node.visibility,
                                             function_type));
    }

    void visit(ExternVarDeclaration& node) override {
        if (context.env.current_scope->lookup_var(node.name) != nullptr ||
            context.env.current_scope->lookup_function(node.name) != nullptr) {
            return;
        }

        if (node.type) {
            visit(*node.type);
        }

        context.env.current_scope->define_var(
            node.name, std::make_unique<VarSymbol>(node.name, node.position, node.visibility,
                                                   StorageKind::Var, node.type->get_type()));
    }

    void visit(ImportStatement& node [[maybe_unused]]) override {}
};
