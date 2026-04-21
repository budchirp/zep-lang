module;

#include <string>
#include <vector>

export module zep.frontend.sema.checker;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.type.type_helper;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;
import zep.frontend.sema.builder;
import zep.frontend.sema.resolver.call;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.resolver.structure;
export class TypeChecker : public Visitor<void> {
  private:
    Context& context;
    TypeContext type_context;

    const FunctionType* current_function = nullptr;

    void define_struct(StructDeclaration& node) {
        if (node.type.is_valid()) {
            return;
        }

        TypeBuilder type_builder(*this);
        TypeId type_id = type_builder.build_struct(node, context.types);
        node.type = type_id;

        TypeSymbol* type_symbol =
            context.symbols.create<TypeSymbol>(node.name, node.span, node.visibility, type_id);
        if (!context.env.current_scope->define_type(node.name, type_symbol)) {
            context.diagnostics.add_error(node.span, "redefinition of type '" + node.name + "'");
        }
    }

    void define_function(FunctionDeclaration& node) {
        if (node.type.is_valid()) {
            return;
        }

        TypeBuilder type_builder(*this);
        TypeId type_id = type_builder.build_function(*node.prototype, context.types);
        node.type = type_id;

        const Type* new_type_ptr = context.types.get(type_id);
        const FunctionType* new_ft = new_type_ptr->as<FunctionType>();
        if (new_ft == nullptr) {
            return;
        }

        for (const FunctionSymbol* existing :
             context.env.current_scope->local_function_overloads(node.prototype->name)) {
            const Type* existing_t = context.types.get(existing->type);
            const FunctionType* existing_ft = existing_t->as<FunctionType>();
            if (existing_ft != nullptr &&
                context.type_helper.conflicts_with(*new_ft, *existing_ft)) {
                context.diagnostics.add_error(node.span, "redefinition of function '" +
                                                             node.prototype->name +
                                                             "' with same parameter types");
                return;
            }
        }

        FunctionSymbol* symbol = context.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, type_id);
        context.env.current_scope->add_function(node.prototype->name, symbol);
    }

    void define_extern_function(ExternFunctionDeclaration& node) {
        if (node.type.is_valid()) {
            return;
        }

        TypeBuilder type_builder(*this);
        TypeId type_id = type_builder.build_function(*node.prototype, context.types);
        node.type = type_id;

        FunctionSymbol* symbol = context.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, type_id);
        context.env.current_scope->add_function(node.prototype->name, symbol);
    }

    void define_variable(VarDeclaration& node) {
        if (node.type.is_valid()) {
            return;
        }

        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        TypeId var_type = TypeId{};
        if (node.annotation != nullptr) {
            var_type = type_context.resolve_type(node.annotation->type);
        }
        node.type = var_type;

        VarSymbol* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                              node.storage_kind, var_type);
        context.env.current_scope->define_var(node.name, symbol);
    }

    void define_extern_variable(ExternVarDeclaration& node) {
        if (node.type.is_valid()) {
            return;
        }

        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        TypeId var_type = TypeId{};
        if (node.annotation != nullptr) {
            var_type = type_context.resolve_type(node.annotation->type);
        }
        node.type = var_type;

        VarSymbol* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                              StorageKind::Type::Var, var_type);
        context.env.current_scope->define_var(node.name, symbol);
    }

    void register_declarations(Program& program) {
        for (Statement* statement : program.statements) {
            StructDeclaration* struct_declaration = statement->as<StructDeclaration>();
            if (struct_declaration != nullptr) {
                define_struct(*struct_declaration);
                continue;
            }

            VarDeclaration* var_declaration = statement->as<VarDeclaration>();
            if (var_declaration != nullptr) {
                define_variable(*var_declaration);
                continue;
            }

            FunctionDeclaration* function_declaration = statement->as<FunctionDeclaration>();
            if (function_declaration != nullptr) {
                define_function(*function_declaration);
                continue;
            }

            ExternFunctionDeclaration* extern_function_declaration =
                statement->as<ExternFunctionDeclaration>();
            if (extern_function_declaration != nullptr) {
                define_extern_function(*extern_function_declaration);
                continue;
            }

            ExternVarDeclaration* extern_var_declaration = statement->as<ExternVarDeclaration>();
            if (extern_var_declaration != nullptr) {
                define_extern_variable(*extern_var_declaration);
            }
        }
    }

    bool is_mutable(Expression& expression) {
        switch (expression.kind) {
        case Expression::Kind::Type::IdentifierExpression: {
            const VarSymbol* symbol =
                context.env.current_scope->lookup_var(expression.as<IdentifierExpression>()->name);
            if (symbol != nullptr) {
                return symbol->storage_kind == StorageKind::Type::VarMut;
            }

            return false;
        }
        case Expression::Kind::Type::UnaryExpression: {
            if (expression.as<UnaryExpression>()->op ==
                UnaryExpression::Operator::Type::Dereference) {
                TypeId type = expression.as<UnaryExpression>()->operand->type;
                if (type.is_valid()) {
                    const Type* operand_type = context.types.get(type);
                    const PointerType* pointer = operand_type->as<PointerType>();
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
        : context(context), type_context(context.types, context.env) {}

    void check(Program& program) {
        register_declarations(program);

        for (Statement* statement : program.statements) {
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
        for (GenericParameter* generic_parameter : node.generic_parameters) {
            visit(*generic_parameter);
        }

        for (Parameter* parameter : node.parameters) {
            visit(*parameter);
        }

        visit(*node.return_type);
    }

    void visit(StructField& node) override { visit(*node.type); }

    void visit(StructLiteralField& node) override { visit_expression(*node.value); }

    void visit(NumberLiteral& node) override {
        node.type = context.env.global_scope->lookup_type("i32");
    }

    void visit(FloatLiteral& node) override {
        node.type = context.env.global_scope->lookup_type("f32");
    }

    void visit(StringLiteral& node) override {
        node.type = context.env.global_scope->lookup_type("string");
    }

    void visit(BooleanLiteral& node) override {
        node.type = context.env.global_scope->lookup_type("bool");
    }

    void visit(IdentifierExpression& node) override {
        const VarSymbol* var = context.env.current_scope->lookup_var(node.name);
        if (var != nullptr) {
            node.type = var->type;
            return;
        }

        std::vector<const FunctionSymbol*> overloads =
            context.env.current_scope->lookup_function_overloads(node.name);
        if (!overloads.empty()) {
            node.type = overloads.front()->type;
            return;
        }

        TypeId type_id = context.env.current_scope->lookup_type(node.name);
        if (type_id.is_valid()) {
            node.type = type_id;
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

            TypeId type = node.right->type;

            if (node.op == Op::As) {
                node.type = type_context.resolve_type(type);
            } else {
                node.type = context.env.global_scope->lookup_type("bool");
            }

            break;
        }
        default: {
            visit_expression(*node.left);
            visit_expression(*node.right);

            TypeId left_type = node.left->type;
            TypeId right_type = node.right->type;

            if (!left_type.is_valid() || !right_type.is_valid()) {
                return;
            }

            const Type* left_type_ptr = context.types.get(left_type);
            const Type* right_type_ptr = context.types.get(right_type);

            switch (node.op) {
            case Op::Plus:
            case Op::Minus:
            case Op::Asterisk:
            case Op::Divide:
            case Op::Modulo: {
                if (!left_type_ptr->is_numeric()) {
                    context.diagnostics.add_error(
                        node.left->span,
                        "left operand of arithmetic operator must be numeric, got '" +
                            context.type_helper.type_to_string(left_type) + "'");
                    return;
                }

                if (!right_type_ptr->is_numeric()) {
                    context.diagnostics.add_error(
                        node.right->span,
                        "right operand of arithmetic operator must be numeric, got '" +
                            context.type_helper.type_to_string(right_type) + "'");
                    return;
                }

                if (!context.type_helper.compatible(left_type, right_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in arithmetic: '" +
                                       context.type_helper.type_to_string(left_type) + "' and '" +
                                       context.type_helper.type_to_string(right_type) + "'");
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
                if (!context.type_helper.compatible(left_type, right_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in comparison: '" +
                                       context.type_helper.type_to_string(left_type) + "' and '" +
                                       context.type_helper.type_to_string(right_type) + "'");
                    return;
                }

                node.type = context.env.global_scope->lookup_type("bool");

                break;
            }
            case Op::And:
            case Op::Or: {
                if (left_type_ptr->kind != Type::Kind::Type::Boolean) {
                    context.diagnostics.add_error(
                        node.left->span, "left operand of logical operator must be boolean, got '" +
                                             context.type_helper.type_to_string(left_type) + "'");
                    return;
                }

                if (right_type_ptr->kind != Type::Kind::Type::Boolean) {
                    context.diagnostics.add_error(
                        node.right->span,
                        "right operand of logical operator must be boolean, got '" +
                            context.type_helper.type_to_string(right_type) + "'");
                    return;
                }

                node.type = context.env.global_scope->lookup_type("bool");

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

        TypeId operand_type = node.operand->type;
        if (!operand_type.is_valid()) {
            return;
        }

        const Type* operand_type_ptr = context.types.get(operand_type);

        switch (node.op) {
        case Op::Plus:
        case Op::Minus: {
            if (!operand_type_ptr->is_numeric()) {
                context.diagnostics.add_error(
                    node.span, "operand of unary +/- must be numeric, got '" +
                                   context.type_helper.type_to_string(operand_type) + "'");
                return;
            }

            node.type = operand_type;

            break;
        }
        case Op::Not: {
            if (operand_type_ptr->kind != Type::Kind::Type::Boolean) {
                context.diagnostics.add_error(
                    node.span, "operand of '!' must be boolean, got '" +
                                   context.type_helper.type_to_string(operand_type) + "'");
                return;
            }

            node.type = context.env.global_scope->lookup_type("bool");

            break;
        }
        case Op::Dereference: {
            const PointerType* pointer = operand_type_ptr->as<PointerType>();
            if (pointer != nullptr) {
                node.type = pointer->element;
            } else {
                context.diagnostics.add_error(
                    node.span, "cannot dereference non-pointer type '" +
                                   context.type_helper.type_to_string(operand_type) + "'");
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
        for (Argument* argument : node.arguments) {
            visit(*argument);
        }

        visit_expression(*node.callee);

        TypeId callee_type = node.callee->type;
        if (!callee_type.is_valid()) {
            return;
        }

        const Type* callee_type_ptr = context.types.get(callee_type);
        const FunctionType* function_type = callee_type_ptr->as<FunctionType>();
        if (function_type == nullptr) {
            context.diagnostics.add_error(
                node.span, "cannot call non-function type '" +
                               context.type_helper.type_to_string(callee_type) + "'");
            return;
        }

        std::vector<const FunctionSymbol*> overloads =
            context.env.current_scope->lookup_function_overloads(function_type->name);

        CallResolver call_resolver(context, type_context, *this);

        if (overloads.size() > 1) {
            const FunctionType* resolved_type =
                call_resolver.resolve_overload(function_type->name, node);
            if (resolved_type == nullptr) {
                return;
            }

            function_type = resolved_type;
        }

        call_resolver.is_valid(function_type, node, true);

        node.type = type_context.resolve_type(function_type->return_type);
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        TypeId value_type = node.value->type;
        if (!value_type.is_valid()) {
            return;
        }

        const Type* value_type_ptr = context.types.get(value_type);
        const ArrayType* array_type = value_type_ptr->as<ArrayType>();
        if (array_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot index non-array type '" +
                                              context.type_helper.type_to_string(value_type) + "'");
            return;
        }

        TypeId index_type = node.index->type;
        if (!index_type.is_valid()) {
            return;
        }

        const Type* index_type_ptr = context.types.get(index_type);
        const IntegerType* integer_type = index_type_ptr->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span,
                                          "array index must be an integer, got '" +
                                              context.type_helper.type_to_string(index_type) + "'");
            return;
        }

        node.type = array_type->element;
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        TypeId value_type = node.value->type;
        if (!value_type.is_valid()) {
            return;
        }

        const Type* value_type_ptr = context.types.get(value_type);
        const StructType* struct_type = value_type_ptr->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot access member of non-struct type '" +
                                              context.type_helper.type_to_string(value_type) + "'");
            return;
        }

        for (const StructFieldType& field : struct_type->fields) {
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

        TypeId target_type = node.target->type;
        TypeId value_type = node.value->type;

        if (!context.type_helper.compatible(value_type, target_type)) {
            context.diagnostics.add_error(
                node.span, "type mismatch in assignment: expected '" +
                               context.type_helper.type_to_string(target_type) + "', got '" +
                               context.type_helper.type_to_string(value_type) + "'");
        }

        node.type = target_type;
    }

    void visit(StructLiteralExpression& node) override {
        visit(*node.name);

        TypeId name_type = node.name->type;
        if (!name_type.is_valid()) {
            return;
        }

        const Type* name_type_ptr = context.types.get(name_type);
        const StructType* struct_type = name_type_ptr->as<StructType>();
        if (struct_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot construct non-struct type '" +
                                              context.type_helper.type_to_string(name_type) + "'");
            return;
        }

        StructResolver struct_resolver(context, type_context, *this);
        struct_resolver.is_valid(struct_type, node);

        node.type = type_context.resolve_type(name_type);
    }

    void visit(BlockStatement& node) override {
        context.env.push_block_scope();

        TypeId last_type = TypeId{};
        for (Statement* statement : node.statements) {
            visit_statement(*statement);

            last_type = statement->type;
        }

        node.type = last_type;

        context.env.pop_scope();
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);

        node.type = node.expression->type;
    }

    void visit(IfExpression& node) override {
        visit_expression(*node.condition);

        TypeId condition_type = node.condition->type;
        if (!condition_type.is_valid()) {
            return;
        }

        const Type* condition_type_ptr = context.types.get(condition_type);
        if (condition_type_ptr->kind != Type::Kind::Type::Boolean) {
            context.diagnostics.add_error(
                node.condition->span, "if condition must be boolean, got '" +
                                          context.type_helper.type_to_string(condition_type) + "'");
        }

        visit_statement(*node.then_branch);

        if (node.else_branch != nullptr) {
            visit_statement(*node.else_branch);

            TypeId then_type = node.then_branch->type;
            TypeId else_type = node.else_branch->type;

            if (!context.type_helper.compatible(then_type, else_type)) {
                context.diagnostics.add_error(
                    node.span, "if/else branches have different types: '" +
                                   context.type_helper.type_to_string(then_type) + "' and '" +
                                   context.type_helper.type_to_string(else_type) + "'");
            }

            node.type = then_type.is_valid() ? then_type : else_type;
        } else {
            node.type = context.env.global_scope->lookup_type("void");
        }
    }

    void visit(ReturnStatement& node) override {
        if (current_function == nullptr) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
        }

        TypeId expected = type_context.resolve_type(current_function->return_type);
        if (!expected.is_valid()) {
            return;
        }

        if (node.value != nullptr) {
            visit_expression(*node.value);

            TypeId type = node.value->type;

            if (!context.type_helper.compatible(type, expected)) {
                context.diagnostics.add_error(
                    node.span, "return type mismatch: expected '" +
                                   context.type_helper.type_to_string(expected) + "', got '" +
                                   context.type_helper.type_to_string(type) + "'");
            }
        } else {
            const Type* expected_ptr = context.types.get(expected);
            if (expected_ptr->kind != Type::Kind::Type::Void) {
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

        TypeId var_type = TypeId{};
        if (node.annotation != nullptr) {
            var_type = type_context.resolve_type(node.annotation->type);
        }
        if (!var_type.is_valid() && node.initializer != nullptr) {
            var_type = node.initializer->type;
        }

        if (node.annotation != nullptr && node.initializer != nullptr && var_type.is_valid()) {
            TypeId init_type = node.initializer->type;

            if (init_type.is_valid() && !context.type_helper.compatible(var_type, init_type)) {
                context.diagnostics.add_error(
                    node.span, "type mismatch in variable declaration: declared '" +
                                   context.type_helper.type_to_string(var_type) +
                                   "', initializer is '" +
                                   context.type_helper.type_to_string(init_type) + "'");
            }
        }

        if (!var_type.is_valid()) {
            context.diagnostics.add_error(node.span,
                                          "cannot determine type of variable '" + node.name + "'");
            var_type = TypeId{};
        }

        if (!context.env.current_scope->has_local_var(node.name)) {
            if (!context.env.current_scope->define_var(
                    node.name,
                    context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                      node.storage_kind, var_type))) {
                context.diagnostics.add_error(node.span, "redefinition of '" + node.name + "'");
            }
        }

        node.type = var_type;
    }

    void visit(FunctionDeclaration& node) override {
        if (context.env.current_scope != context.env.global_scope) {
            context.diagnostics.add_error(node.span,
                                          "functions can only be declared in global scope");
            return;
        }

        define_function(node);
        const Type* ft_ptr = context.types.get(node.type);
        const FunctionType* function_type = ft_ptr->as<FunctionType>();

        const FunctionType* previous_function_type = current_function;
        current_function = function_type;

        TypeContext::SubstitutionScope substitution_scope = type_context.scoped_substitutions();
        (void)substitution_scope;
        GenericResolver generic_resolver(context, type_context, *this);
        generic_resolver.define_generic_parameters(function_type->generic_parameters);

        context.env.push_scope(context.env.current_scope->name + "." + node.prototype->name);

        for (const ParameterType& parameter : function_type->parameters) {
            TypeId param_type = type_context.resolve_type(parameter.type);
            context.env.current_scope->define_var(
                parameter.name, context.symbols.create<VarSymbol>(
                                    parameter.name, node.span, Visibility::Type::Private,
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
