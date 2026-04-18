module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.frontend.sema.checker.type_checker;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kinds;
import zep.frontend.sema.checker.type_builder;
import zep.frontend.sema.checker.call_resolver;
import zep.frontend.sema.checker.generic_resolver;
import zep.frontend.sema.checker.struct_resolver;
export class TypeChecker : public Visitor<void> {
  private:
    Context& context;
    TypeContext type_context;

    const FunctionType* current_function = nullptr;

    void define_struct(StructDeclaration& node) {
        if (node.get_type() != nullptr) {
            return;
        }

        TypeBuilder type_builder(*this);
        auto type = type_builder.build_struct(node);
        node.set_type(type);

        context.env.current_scope->define_type(node.name, std::move(type));
    }

    void define_function(FunctionDeclaration& node) {
        if (node.get_type() != nullptr) {
            return;
        }

        TypeBuilder type_builder(*this);
        auto type = type_builder.build_function(*node.prototype);
        node.set_type(type);

        if (!context.env.current_scope->define_function(
                node.prototype->name,
                std::make_unique<FunctionSymbol>(node.prototype->name, node.span,
                                                 node.visibility, type))) {
            context.diagnostics.add_error(node.span, "redefinition of function '" +
                                                             node.prototype->name +
                                                             "' with same parameter types");
        }
    }

    void define_extern_function(ExternFunctionDeclaration& node) {
        if (node.get_type() != nullptr) {
            return;
        }

        TypeBuilder type_builder(*this);
        auto type = type_builder.build_function(*node.prototype);
        node.set_type(type);

        context.env.current_scope->define_function(
            node.prototype->name, std::make_unique<FunctionSymbol>(
                                      node.prototype->name, node.span, node.visibility, type));
    }

    void define_variable(VarDeclaration& node) {
        if (node.get_type() != nullptr) {
            return;
        }

        if (node.type) {
            visit(*node.type);
        }

        auto type = node.type ? node.type->get_type() : nullptr;
        node.set_type(type);

        context.env.current_scope->define_var(
            node.name, std::make_unique<VarSymbol>(node.name, node.span, node.visibility,
                                                   node.storage_kind, type));
    }

    void define_extern_variable(ExternVarDeclaration& node) {
        if (node.get_type() != nullptr) {
            return;
        }

        if (node.type) {
            visit(*node.type);
        }

        auto type = node.type ? node.type->get_type() : nullptr;
        node.set_type(type);

        context.env.current_scope->define_var(
            node.name, std::make_unique<VarSymbol>(node.name, node.span, node.visibility,
                                                   StorageKind::Type::Var, type));
    }

    void register_declarations(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* node = statement->as<StructDeclaration>()) {
                define_struct(*node);
            } else if (auto* node = statement->as<VarDeclaration>()) {
                define_variable(*node);
            } else if (auto* node = statement->as<FunctionDeclaration>()) {
                define_function(*node);
            } else if (auto* node = statement->as<ExternFunctionDeclaration>()) {
                define_extern_function(*node);
            } else if (auto* node = statement->as<ExternVarDeclaration>()) {
                define_extern_variable(*node);
            }
        }
    }

    bool is_mutable(Expression& expression) {
        switch (expression.kind) {
        case Expression::Kind::Type::IdentifierExpression: {
            const auto* symbol =
                context.env.current_scope->lookup_var(expression.as<IdentifierExpression>()->name);
            if (symbol != nullptr) {
                return symbol->storage_kind == StorageKind::Type::VarMut;
            }

            return false;
        }
        case Expression::Kind::Type::UnaryExpression: {
            if (expression.as<UnaryExpression>()->op == UnaryExpression::Operator::Type::Dereference) {
                auto type = expression.as<UnaryExpression>()->operand->get_type();
                if (type != nullptr) {
                    if (auto* pointer = type->as<PointerType>()) {
                        return pointer->is_mutable;
                    }
                }
            }

            return false;
        }
        case Expression::Kind::Type::MemberExpression: {
            return is_mutable(*expression.as<MemberExpression>()->value);
        }
        case Expression::Kind::Type::IndexExpression: {
            return is_mutable(*expression.as<IndexExpression>()->value);
        }
        default: {
            return false;
        }
        }
    }

  public:
    explicit TypeChecker(Context& context) : context(context), type_context(context.env) {}

    void check(Program& program) {
        register_declarations(program);

        for (auto& statement : program.statements) {
            visit_statement(*statement);
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
        const auto* var = context.env.current_scope->lookup_var(node.name);
        if (var != nullptr) {
            node.set_type(var->type);
            return;
        }

        const auto* function = context.env.current_scope->lookup_function(node.name);
        if (function != nullptr) {
            node.set_type(function->type);
            return;
        }

        auto type = context.env.current_scope->lookup_type(node.name);
        if (type != nullptr) {
            node.set_type(type);
            return;
        }

        context.diagnostics.add_error(node.span,
                                      "use of undeclared symbol '" + node.name + "'");
    }

    void visit(BinaryExpression& node) override {
        using Op = BinaryExpression::Operator::Type;

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
                        node.left->span,
                        "left operand of arithmetic operator must be numeric, got '" +
                            left_type->to_string() + "'");
                    return;
                }

                if (!right_type->is_numeric()) {
                    context.diagnostics.add_error(
                        node.right->span,
                        "right operand of arithmetic operator must be numeric, got '" +
                            right_type->to_string() + "'");
                    return;
                }

                if (!Type::compatible(left_type, right_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in arithmetic: '" + left_type->to_string() +
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
                        node.span, "type mismatch in comparison: '" + left_type->to_string() +
                                           "' and '" + right_type->to_string() + "'");
                    return;
                }

                node.set_type(std::make_shared<BooleanType>());

                break;
            }
            case Op::And:
            case Op::Or: {
                if (left_type->kind != Type::Kind::Type::Boolean) {
                    context.diagnostics.add_error(
                        node.left->span,
                        "left operand of logical operator must be boolean, got '" +
                            left_type->to_string() + "'");
                    return;
                }

                if (right_type->kind != Type::Kind::Type::Boolean) {
                    context.diagnostics.add_error(
                        node.right->span,
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
        using Op = UnaryExpression::Operator::Type;

        visit_expression(*node.operand);

        auto operand_type = node.operand->get_type();
        if (!operand_type) {
            return;
        }

        switch (node.op) {
        case Op::Plus:
        case Op::Minus: {
            if (!operand_type->is_numeric()) {
                context.diagnostics.add_error(node.span,
                                              "operand of unary +/- must be numeric, got '" +
                                                  operand_type->to_string() + "'");
                return;
            }

            node.set_type(operand_type);

            break;
        }
        case Op::Not: {
            if (operand_type->kind != Type::Kind::Type::Boolean) {
                context.diagnostics.add_error(node.span,
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
                context.diagnostics.add_error(node.span,
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
        for (auto& argument : node.arguments) {
            visit(*argument);
        }

        visit_expression(*node.callee);

        auto callee_type = node.callee->get_type();
        if (!callee_type) {
            return;
        }

        const auto* function_type = callee_type->as<FunctionType>();
        if (function_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot call non-function type '" +
                                                             callee_type->to_string() + "'");
            return;
        }

        auto overloads = context.env.current_scope->lookup_function_overloads(function_type->name);

        CallResolver call_resolver(context, type_context, *this);

        if (overloads.size() > 1) {
            const auto* resolved_type =
                call_resolver.resolve_overload(function_type->name, node);
            if (resolved_type == nullptr) {
                return;
            }

            function_type = resolved_type;
        }

        call_resolver.is_valid(function_type, node, true);

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
            context.diagnostics.add_error(node.span, "cannot index non-array type '" +
                                                             value_type->to_string() + "'");
            return;
        }

        auto index_type = node.index->get_type();
        if (!index_type) {
            return;
        }

        auto* integer_type = index_type->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span,
                                          "array index must be an integer, got '" +
                                              index_type->to_string() + "'");
            return;
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
            context.diagnostics.add_error(node.span,
                                          "cannot access member of non-struct type '" +
                                              value_type->to_string() + "'");
            return;
        }

        for (const auto& field : struct_type->fields) {
            if (field->name == node.member) {
                node.set_type(field->type);
                return;
            }
        }

        context.diagnostics.add_error(node.span, "struct '" + struct_type->name +
                                                         "' has no member '" + node.member + "'");
    }

    void visit(AssignExpression& node) override {
        visit_expression(*node.target);
        visit_expression(*node.value);

        if (!is_mutable(*node.target)) {
            context.diagnostics.add_error(node.target->span,
                                          "cannot assign to immutable target");
        }

        auto target_type = node.target->get_type();
        auto value_type = node.value->get_type();

        if (!Type::compatible(value_type, target_type)) {
            context.diagnostics.add_error(node.span, "type mismatch in assignment: expected '" +
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
            context.diagnostics.add_error(node.span, "cannot construct non-struct type '" +
                                                             name_type->to_string() + "'");
            return;
        }

        StructResolver struct_resolver(context, type_context, *this);
        struct_resolver.is_valid(struct_type, node);

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

        if (condition_type->kind != Type::Kind::Type::Boolean) {
            context.diagnostics.add_error(node.condition->span,
                                          "if condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch) {
            visit_statement(*node.else_branch);

            auto then_type = node.then_branch->get_type();
            auto else_type = node.else_branch->get_type();

            if (!Type::compatible(then_type, else_type)) {
                context.diagnostics.add_error(node.span,
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
        if (!current_function) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
        }

        auto expected = type_context.resolve_type(current_function->return_type);
        if (!expected) {
            return;
        }

        if (node.value) {
            visit_expression(*node.value);

            auto type = node.value->get_type();

            if (!Type::compatible(type, expected)) {
                context.diagnostics.add_error(
                    node.span, "return type mismatch: expected '" + expected->to_string() +
                                       "', got '" + type->to_string() + "'");
            }
        } else if (expected->kind != Type::Kind::Type::Void) {
            context.diagnostics.add_error(node.span, "non-void function must return a value");
        }
    }

    void visit(StructDeclaration& node) override { define_struct(node); }

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
                context.diagnostics.add_error(node.span,
                                              "type mismatch in variable declaration: declared '" +
                                                  var_type->to_string() + "', initializer is '" +
                                                  init_type->to_string() + "'");
            }
        }

        if (!var_type) {
            context.diagnostics.add_error(node.span,
                                          "cannot determine type of variable '" + node.name + "'");
            var_type = std::make_shared<UnknownType>();
        }

        if (node.get_type() == nullptr) {
            if (!context.env.current_scope->define_var(
                    node.name,
                    std::make_unique<VarSymbol>(node.name, node.span, node.visibility,
                                                node.storage_kind, var_type))) {
                context.diagnostics.add_error(node.span, "redefinition of '" + node.name + "'");
            }
        }

        node.set_type(var_type);
    }

    void visit(FunctionDeclaration& node) override {
        if (context.env.current_scope != context.env.global_scope.get()) {
            context.diagnostics.add_error(node.span,
                                          "functions can only be declared in global scope");
            return;
        }

        define_function(node);
        const auto* function_type = node.get_type()->as<FunctionType>();

        auto previous_function_type = current_function;
        current_function = function_type;

        auto scope = type_context.scoped_substitutions();
        GenericResolver generic_resolver(context, type_context, *this);
        generic_resolver.define_generic_parameters(function_type->generic_parameters);

        context.env.push_scope(context.env.current_scope->name + "." + node.prototype->name);

        for (const auto& parameter : function_type->parameters) {
            auto param_type = type_context.resolve_type(parameter->type);
            context.env.current_scope->define_var(
                parameter->name, std::make_unique<VarSymbol>(parameter->name, node.span,
                                                             Visibility::Type::Private,
                                                             StorageKind::Type::Var, param_type));
        }

        visit(*node.body);

        context.env.pop_scope();

        current_function = previous_function_type;
    }

    void visit(ExternFunctionDeclaration& node) override { define_extern_function(node); }

    void visit(ExternVarDeclaration& node) override { define_extern_variable(node); }

    void visit(ImportStatement& node [[maybe_unused]]) override {}
};
