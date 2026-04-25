module;

#include <optional>
#include <string>
#include <vector>

export module zep.frontend.sema.type.checker;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.type.helper;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.resolver.call;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.resolver.structure;

export class TypeChecker : public Visitor<void> {
  private:
    Context& context;

    TypeResolver resolver;
    TypeBuilder builder;
    TypeHelper helper;

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
            if (expression.as<UnaryExpression>()->op ==
                UnaryExpression::Operator::Type::Dereference) {
                const auto* type = expression.as<UnaryExpression>()->operand->type;
                if (type != nullptr) {
                    const auto* operand_type = type;
                    const auto* pointer = operand_type->as<PointerType>();
                    if (pointer != nullptr) {
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
    explicit TypeChecker(Context& context)
        : context(context), resolver(context.types, context.env), builder(*this, context, resolver),
          helper(*this, context, resolver, builder) {}

    void check(Program& program) {
        helper.register_declarations(program);

        for (auto* statement : program.statements) {
            visit_statement(*statement);
        }
    }

    void visit(TypeExpression& node) override { node.type = resolver.resolve_type(node.type); }

    void visit(GenericParameter& node) override {
        if (node.constraint != nullptr) {
            visit(*node.constraint);
        }
    }

    void visit(GenericArgument& node) override { visit(*node.type); }

    void visit(Parameter& node) override { visit(*node.type); }

    void visit(Argument& node) override { visit_expression(*node.value); }

    void visit(FunctionPrototype& node) override {
        for (auto* generic_parameter : node.generic_parameters) {
            visit(*generic_parameter);
        }

        for (auto* parameter : node.parameters) {
            visit(*parameter);
        }

        visit(*node.return_type);
    }

    void visit(StructField& node) override { visit(*node.type); }

    void visit(StructLiteralField& node) override { visit_expression(*node.value); }

    void visit(NumberLiteral& node) override { node.type = context.env.primitives["i32"]; }

    void visit(FloatLiteral& node) override { node.type = context.env.primitives["f64"]; }

    void visit(StringLiteral& node) override { node.type = context.env.primitives["string"]; }

    void visit(BooleanLiteral& node) override { node.type = context.env.primitives["boolean"]; }

    void visit(IdentifierExpression& node) override {
        const auto* var_symbol = context.env.current_scope->lookup_var(node.name);
        if (var_symbol != nullptr) {
            node.type = var_symbol->type;
            return;
        }

        const auto* type_symbol = context.env.current_scope->lookup_type(node.name);
        if (type_symbol != nullptr) {
            node.type = type_symbol->type;
            return;
        }

        const auto& overloads = context.env.current_scope->lookup_function(node.name);
        if (!overloads.empty()) {
            node.type = overloads.front()->type;
            return;
        }

        context.diagnostics.add_error(node.span, "use of undeclared symbol '" + node.name + "'");
    }

    void visit(BinaryExpression& node) override {
        using Op = BinaryExpression::Operator::Type;

        visit_expression(*node.left);
        visit_expression(*node.right);

        const auto* left_type = node.left->type;
        const auto* right_type = node.right->type;

        if (left_type == nullptr || right_type == nullptr) {
            return;
        }

        switch (node.op) {
        case Op::As:
        case Op::Is: {
            if (node.op == Op::As) {
                node.type = resolver.resolve_type(right_type);
            } else {
                node.type = context.types.create<BooleanType>();
            }

            break;
        }
        case Op::Plus:
        case Op::Minus:
        case Op::Asterisk:
        case Op::Divide:
        case Op::Modulo:
        case Op::LessThan:
        case Op::GreaterThan:
        case Op::LessEqual:
        case Op::GreaterEqual: {
            if (!left_type->is_numeric()) {
                context.diagnostics.add_error(
                    node.left->span, "left operand of arithmetic operator must be numeric, got '" +
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

            if (!left_type->compatible(right_type)) {
                context.diagnostics.add_error(node.span, "type mismatch in arithmetic: '" +
                                                             left_type->to_string() + "' and '" +
                                                             right_type->to_string() + "'");
                return;
            }

            node.type = left_type;

            break;
        }
        case Op::Equals:
        case Op::NotEquals: {
            if (!left_type->compatible(right_type)) {
                context.diagnostics.add_error(node.span, "type mismatch in comparison: '" +
                                                             left_type->to_string() + "' and '" +
                                                             right_type->to_string() + "'");
                return;
            }

            node.type = context.types.create<BooleanType>();

            break;
        }
        case Op::And:
        case Op::Or: {
            if (left_type->is<BooleanType>() && right_type->is<BooleanType>()) {
                node.type = context.types.create<BooleanType>();
                break;
            }

            context.diagnostics.add_error(node.span, "type mismatch in logical operator: '" +
                                                         left_type->to_string() + "' and '" +
                                                         right_type->to_string() + "'");
            break;
        }
        default:
            break;
        }
    }

    void visit(UnaryExpression& node) override {
        using Op = UnaryExpression::Operator::Type;

        visit_expression(*node.operand);

        const auto* operand_type = node.operand->type;
        if (operand_type == nullptr) {
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

            node.type = operand_type;

            break;
        }
        case Op::Not: {
            if (!operand_type->is<BooleanType>()) {
                context.diagnostics.add_error(node.span, "operand of '!' must be boolean, got '" +
                                                             operand_type->to_string() + "'");
                return;
            }

            node.type = context.types.create<BooleanType>();

            break;
        }
        case Op::Dereference: {
            const auto* pointer = operand_type->as<PointerType>();
            if (pointer == nullptr) {
                context.diagnostics.add_error(node.span, "cannot dereference non-pointer type '" +
                                                             operand_type->to_string() + "'");
                return;
            }

            node.type = pointer->element;

            break;
        }
        case Op::AddressOf: {
            node.type = context.types.create<PointerType>(operand_type, false);

            break;
        }
        }
    }

    void visit(CallExpression& node) override {
        visit_expression(*node.callee);

        for (auto* argument : node.arguments) {
            visit(*argument);
        }

        const auto* callee_type = node.callee->type;
        if (callee_type == nullptr) {
            return;
        }

        const auto* function_type = callee_type->as<FunctionType>();
        if (function_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot call non-function type '" +
                                                         node.callee->type->to_string() + "'");
            return;
        }

        auto scope = resolver.scoped_substitutions();

        const auto& overloads = context.env.current_scope->lookup_function(function_type->name);
        CallResolver call_resolver(context, resolver, *this, node);
        if (overloads.size() > 1) {
            function_type = call_resolver.resolve_overload(function_type->name);
        }

        if (function_type != nullptr) {
            call_resolver.is_valid(function_type, true);
            node.type = resolver.resolve_type(function_type->return_type);
        }
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        const auto* value_type = node.value->type;
        const auto* index_type = node.index->type;
        if (value_type == nullptr || index_type == nullptr) {
            return;
        }

        const auto* array_type = value_type->as<ArrayType>();
        if (array_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot index non-array type '" +
                                                         value_type->to_string() + "'");
            return;
        }

        const auto* integer_type = index_type->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span,
                                          "array index must be an integer, got '" +
                                              index_type->to_string() + "'");
            return;
        }

        node.type = array_type->element;
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        const auto* value_type = node.value->type;
        if (value_type == nullptr) {
            return;
        }

        const auto* struct_type = value_type->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot access member of non-struct type '" +
                                                         node.value->type->to_string() + "'");
            return;
        }

        for (const auto& field : struct_type->fields) {
            if (field.name == node.member) {
                node.type = resolver.resolve_type(field.type);
                return;
            }
        }

        context.diagnostics.add_error(node.span, "struct '" + struct_type->name +
                                                     "' has no member '" + node.member + "'");
    }

    void visit(AssignExpression& node) override {
        visit_expression(*node.target);
        visit_expression(*node.value);

        const auto* target_type = node.target->type;
        const auto* value_type = node.value->type;
        if (target_type == nullptr || value_type == nullptr) {
            return;
        }

        if (!is_mutable(*node.target)) {
            context.diagnostics.add_error(node.target->span, "cannot assign to immutable target");
        }

        if (!value_type->compatible(target_type)) {
            context.diagnostics.add_error(node.span, "type mismatch in assignment: expected '" +
                                                         target_type->to_string() + "', got '" +
                                                         value_type->to_string() + "'");
        }

        node.type = target_type;
    }

    void visit(StructLiteralExpression& node) override {
        visit(*node.name);

        const auto* struct_type = node.name->type->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot construct non-struct type '" +
                                                         node.name->type->to_string() + "'");
            return;
        }

        StructureResolver structure_resolver(context, resolver, *this, node);
        structure_resolver.is_valid(struct_type);

        node.type = resolver.resolve_type(struct_type);
    }

    void visit(BlockStatement& node) override {
        const Type* return_type = nullptr;
        for (auto* statement : node.statements) {
            visit_statement(*statement);

            return_type = statement->type;
        }

        node.type = (return_type == nullptr) ? context.env.primitives["void"] : return_type;
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);

        node.type = node.expression->type;
    }

    void visit(IfExpression& node) override {
        visit_expression(*node.condition);

        const auto* condition_type = node.condition->type;
        if (condition_type == nullptr) {
            return;
        }

        if (!condition_type->is<BooleanType>()) {
            context.diagnostics.add_error(node.condition->span,
                                          "if condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch != nullptr) {
            visit_statement(*node.else_branch);

            const auto* then_type = node.then_branch->type;
            const auto* else_type = node.else_branch->type;
            if (then_type == nullptr || else_type == nullptr) {
                return;
            }

            if (!then_type->compatible(else_type)) {
                context.diagnostics.add_error(
                    node.span, "if/else branches have different types: '" + then_type->to_string() +
                                   "' and '" + else_type->to_string() + "'");
            }

            node.type = then_type;
        } else {
            node.type = context.env.primitives["void"];
        }
    }

    void visit(ReturnStatement& node) override {
        if (helper.current_function == nullptr) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
            return;
        }

        const auto* return_type =
            resolver.resolve_type(helper.current_function->function_type->return_type);
        if (return_type == nullptr) {
            return;
        }

        if (node.value != nullptr) {
            visit_expression(*node.value);

            const auto* value_type = node.value->type;
            if (value_type == nullptr) {
                context.diagnostics.add_error(node.span, "return value has no type");
                return;
            }

            if (!value_type->compatible(return_type)) {
                context.diagnostics.add_error(node.span, "return type mismatch: expected '" +
                                                             return_type->to_string() + "', got '" +
                                                             value_type->to_string() + "'");
            }
        } else {
            if (!return_type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "non-void function must return a value");
            }
        }
    }

    void visit(StructDeclaration& node) override { helper.define_struct(node); }

    void visit(VarDeclaration& node) override {
        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        if (node.initializer != nullptr) {
            visit_expression(*node.initializer);
        }

        const Type* var_type = nullptr;
        if (node.annotation != nullptr) {
            var_type = resolver.resolve_type(node.annotation->type);
        }

        if (node.initializer != nullptr) {
            if (var_type == nullptr) {
                var_type = node.initializer->type;
            } else {
                const auto* init_type = node.initializer->type;
                if (init_type != nullptr && !var_type->compatible(init_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in variable declaration: declared '" +
                                       var_type->to_string() + "', initializer is '" +
                                       init_type->to_string() + "'");
                }
            }
        }

        if (var_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot determine type of variable '" + node.name + "'");
        } else if (!context.env.current_scope->has_variable(node.name)) {
            context.env.current_scope->define_var(
                node.name, context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                             node.storage_kind, var_type));
        }

        node.type = var_type;
    }

    void visit(FunctionDeclaration& node) override {
        if (!context.env.current_scope->is_global()) {
            context.diagnostics.add_error(node.span,
                                          "functions can only be declared in global scope");
            return;
        }

        auto* symbol = helper.define_function(node);
        helper.current_function = symbol;

        auto scope = resolver.scoped_substitutions();
        GenericResolver generic_resolver(context, resolver, *this);
        generic_resolver.activate_generic_parameters(symbol->function_type->generic_parameters);

        for (const auto& parameter : symbol->function_type->parameters) {
            const auto* parameter_type = resolver.resolve_type(parameter.type);
            if (parameter_type == nullptr) {
                context.diagnostics.add_error(node.span, "cannot determine type of parameter '" +
                                                             parameter.name + "'");
            }

            context.env.current_scope->define_var(
                parameter.name, context.symbols.create<VarSymbol>(
                                    parameter.name, node.span, Visibility::Type::Private,
                                    StorageKind::Type::Var, parameter_type));
        }

        visit(*node.body);
        helper.current_function = nullptr;
    }

    void visit(ExternFunctionDeclaration& node) override { helper.define_extern_function(node); }

    void visit(ExternVarDeclaration& node) override { helper.define_extern_variable(node); }

    void visit(ImportStatement& node [[maybe_unused]]) override {}
};
