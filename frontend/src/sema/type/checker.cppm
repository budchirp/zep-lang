module;

#include <optional>
#include <string>
#include <vector>

export module zep.frontend.sema.type.checker;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
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
    TypeContext type_context;

    TypeBuilder builder;

    // ---

    const FunctionType* current_function = nullptr;

    // ---

    void define_struct(StructDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        const auto* type = builder.build_struct(node);
        node.type = type;

        auto* symbol =
            context.symbols.create<TypeSymbol>(node.name, node.span, node.visibility, type);

        if (!context.env.current_scope->define_type(node.name, symbol)) {
            context.diagnostics.add_error(node.span, "redefinition of type '" + node.name + "'");
        }
    }

    FunctionSymbol* define_function(FunctionDeclaration& node) {
        if (node.type != nullptr) {
            for (auto* symbol :
                 context.env.current_scope->lookup_function(node.prototype->name)) {
                if (symbol->type == node.type) {
                    return symbol;
                }
            }

            return nullptr;
        }

        const auto* type = builder.build_function(*node.prototype);
        node.type = type;

        for (auto* symbol :
             context.env.current_scope->lookup_function(node.prototype->name)) {
            const auto* function_type = symbol->type->as<FunctionType>();
            if (function_type == nullptr) {
                continue;
            }

            if (function_type->conflicts_with(*type)) {
                context.diagnostics.add_warning(node.span, "duplicate signature for function '" +
                                                               node.prototype->name + "'");
                break;
            }
        }

        auto* symbol = context.symbols.create<FunctionSymbol>(node.prototype->name, node.span,
                                                              node.visibility, type);

        context.env.current_scope->define_function(node.prototype->name, symbol);

        return symbol;
    }

    void define_extern_function(ExternFunctionDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        const auto* type = builder.build_function(*node.prototype);
        node.type = type;

        auto* symbol = context.symbols.create<FunctionSymbol>(node.prototype->name, node.span,
                                                              node.visibility, type);
        context.env.current_scope->define_function(node.prototype->name, symbol);
    }

    void define_variable(VarDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        const Type* type = nullptr;
        if (node.annotation != nullptr) {
            type = type_context.resolve_type(node.annotation->type);
        }
        node.type = type;

        auto* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                         node.storage_kind, type);

        if (!context.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }

    void define_extern_variable(ExternVarDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        visit(*node.annotation);

        const auto* type = type_context.resolve_type(node.annotation->type);
        node.type = type;

        auto* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                         StorageKind::Type::Var, type);

        if (!context.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }

    // ---

    void register_declarations(Program& program) {
        for (auto* statement : program.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                define_struct(*struct_declaration);
                continue;
            }

            if (auto* var_declaration = statement->as<VarDeclaration>();
                var_declaration != nullptr) {
                define_variable(*var_declaration);
                continue;
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                define_function(*function_declaration);
                continue;
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                define_extern_function(*extern_function_declaration);
                continue;
            }

            if (auto* extern_var_declaration = statement->as<ExternVarDeclaration>();
                extern_var_declaration != nullptr) {
                define_extern_variable(*extern_var_declaration);
            }
        }
    }

    // ---

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
        : context(context), type_context(context.types, context.env), builder(*this, context) {}

    void check(Program& program) {
        register_declarations(program);

        for (auto* statement : program.statements) {
            visit_statement(*statement);
        }
    }

    void visit(TypeExpression& node) override { node.type = type_context.resolve_type(node.type); }

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

    void visit(NumberLiteral& node) override {
        node.type = context.env.primitives["i32"];
    }

    void visit(FloatLiteral& node) override { node.type = context.env.primitives["f64"]; }

    void visit(StringLiteral& node) override { node.type = context.env.primitives["string"]; }

    void visit(BooleanLiteral& node) override { node.type = context.env.primitives["boolean"]; }

    void visit(IdentifierExpression& node) override {
        const auto* var = context.env.current_scope->lookup_var(node.name);
        if (var != nullptr) {
            node.type = var->type;
            return;
        }

        const auto& overloads = context.env.current_scope->lookup_function(node.name);
        if (!overloads.empty()) {
            node.type = overloads.front()->type;
            return;
        }

        const auto* symbol = context.env.current_scope->lookup_type(node.name);
        if (symbol != nullptr) {
            node.type = symbol->type;
            return;
        }

        context.diagnostics.add_error(node.span, "use of undeclared symbol '" + node.name + "'");
    }

    void visit(BinaryExpression& node) override {
        using Op = BinaryExpression::Operator::Type;

        switch (node.op) {
        case Op::As:
        case Op::Is: {
            visit_expression(*node.left);

            const auto* type = node.right->type;

            if (node.op == Op::As) {
                node.type = type_context.resolve_type(type);
            } else {
                node.type = context.types.create<BooleanType>();
            }

            break;
        }
        default: {
            visit_expression(*node.left);
            visit_expression(*node.right);

            const auto* left_type = node.left->type;
            const auto* right_type = node.right->type;

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

                if (!left_type->compatible(right_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in arithmetic: '" + left_type->to_string() +
                                       "' and '" + right_type->to_string() + "'");
                    return;
                }

                node.type = left_type;

                break;
            }
            case Op::Equals:
            case Op::NotEquals:
            case Op::LessThan:
            case Op::GreaterThan:
            case Op::LessEqual:
            case Op::GreaterEqual: {
                if (!left_type->compatible(right_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in comparison: '" + left_type->to_string() +
                                       "' and '" + right_type->to_string() + "'");
                    return;
                }

                node.type = context.types.create<BooleanType>();

                break;
            }
            case Op::And:
            case Op::Or: {
                if (left_type->kind != Type::Kind::Type::Boolean) {
                    context.diagnostics.add_error(
                        node.left->span, "left operand of logical operator must be boolean, got '" +
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

                node.type = context.types.create<BooleanType>();

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

        const auto* operand_type = node.operand->type;

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
            if (operand_type->kind != Type::Kind::Type::Boolean) {
                context.diagnostics.add_error(node.span, "operand of '!' must be boolean, got '" +
                                                             operand_type->to_string() + "'");
                return;
            }

            node.type = context.types.create<BooleanType>();

            break;
        }
        case Op::Dereference: {
            const auto* pointer = operand_type->as<PointerType>();
            if (pointer != nullptr) {
                node.type = pointer->element;
            } else {
                context.diagnostics.add_error(node.span, "cannot dereference non-pointer type '" +
                                                             operand_type->to_string() + "'");
            }

            break;
        }
        case Op::AddressOf: {
            node.type = context.types.create<PointerType>(operand_type, false);

            break;
        }
        }
    }

    void visit(CallExpression& node) override {
        for (auto* argument : node.arguments) {
            visit(*argument);
        }

        visit_expression(*node.callee);

        const auto* function_type = node.callee->type->as<FunctionType>();
        if (function_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot call non-function type '" +
                                                         node.callee->type->to_string() + "'");
            return;
        }

        const auto& overloads =
            context.env.current_scope->lookup_function(function_type->name);

        CallResolver call_resolver(context, type_context, *this);
        if (overloads.size() > 1) {
            function_type = call_resolver.resolve_overload(function_type->name, node);
        }

        call_resolver.is_valid(function_type, node, true);

        node.type = type_context.resolve_type(function_type->return_type);
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        const auto* array_type = node.value->type->as<ArrayType>();
        if (array_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot index non-array type '" +
                                                         node.value->type->to_string() + "'");
            return;
        }

        const auto* integer_type = node.index->type->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span,
                                          "array index must be an integer, got '" +
                                              node.index->type->to_string() + "'");
            return;
        }

        node.type = array_type->element;
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        const auto* struct_type = node.value->type->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot access member of non-struct type '" +
                                                         node.value->type->to_string() + "'");
            return;
        }

        for (const auto& field : struct_type->fields) {
            if (field.name == node.member) {
                node.type = field.type;
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
            context.diagnostics.add_error(node.target->span, "cannot assign to immutable target");
        }

        const auto* target_type = node.target->type;
        const auto* value_type = node.value->type;

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

        StructureResolver structure_resolver(context, type_context, *this);
        structure_resolver.is_valid(struct_type, node);

        node.type = type_context.resolve_type(struct_type);
    }

    void visit(BlockStatement& node) override {
        const Type* return_type = nullptr;
        for (auto* statement : node.statements) {
            visit_statement(*statement);

            return_type = statement->type;
        }

        node.type = (return_type == nullptr) ? context.types.create<VoidType>() : return_type;
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);

        node.type = node.expression->type;
    }

    void visit(IfExpression& node) override {
        visit_expression(*node.condition);

        const auto* condition_type = node.condition->type;
        if (condition_type->kind != Type::Kind::Type::Boolean) {
            context.diagnostics.add_error(node.condition->span,
                                          "if condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch != nullptr) {
            visit_statement(*node.else_branch);

            const auto* then_type = node.then_branch->type;
            const auto* else_type = node.else_branch->type;

            if (!then_type->compatible(else_type)) {
                context.diagnostics.add_error(
                    node.span, "if/else branches have different types: '" + then_type->to_string() +
                                   "' and '" + else_type->to_string() + "'");
            }

            node.type = (then_type != nullptr) ? then_type : else_type;
        } else {
            node.type = context.types.create<VoidType>();
        }
    }

    void visit(ReturnStatement& node) override {
        if (current_function == nullptr) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
            return;
        }

        const auto* expected = type_context.resolve_type(current_function->return_type);
        if (expected == nullptr) {
            return;
        }

        if (node.value != nullptr) {
            visit_expression(*node.value);

            const auto* type = node.value->type;

            if (type == nullptr) {
                context.diagnostics.add_error(node.span, "return value has no type");
            } else if (!type->compatible(expected)) {
                context.diagnostics.add_error(node.span, "return type mismatch: expected '" +
                                                             expected->to_string() + "', got '" +
                                                             type->to_string() + "'");
            }
        } else {
            if (expected->kind != Type::Kind::Type::Void) {
                context.diagnostics.add_error(node.span, "non-void function must return a value");
            }
        }
    }

    void visit(StructDeclaration& node) override { define_struct(node); }

    void visit(VarDeclaration& node) override {
        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        if (node.initializer != nullptr) {
            visit_expression(*node.initializer);
        }

        const Type* var_type = nullptr;
        if (node.annotation != nullptr) {
            var_type = type_context.resolve_type(node.annotation->type);
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

        auto* function_symbol = define_function(node);
        const auto* function_type = function_symbol->type->as<FunctionType>();

        current_function = function_type;

        GenericResolver generic_resolver(context, type_context, *this);
        generic_resolver.define_generic_parameters(function_type->generic_parameters);

        for (const auto& parameter : function_type->parameters) {
            const auto* parameter_type = type_context.resolve_type(parameter.type);
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
    }

    void visit(ExternFunctionDeclaration& node) override { define_extern_function(node); }

    void visit(ExternVarDeclaration& node) override { define_extern_variable(node); }

    void visit(ImportStatement& node [[maybe_unused]]) override {}
};
