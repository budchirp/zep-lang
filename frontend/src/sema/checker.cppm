module;

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

export module zep.frontend.sema.checker;

import zep.frontend.sema.type;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.common.source.span;
import zep.common.diagnostic.diagnostic;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.type.coercion;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.declaration;
import zep.frontend.sema.resolver.call;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.resolver.literal;
import zep.frontend.sema.resolver.member;
import zep.frontend.sema.resolver.operator_resolver;
import zep.frontend.sema.resolver.when;
import zep.common.context;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.kind;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.resolver.builtin;

export class SemanticChecker : public Visitor<void> {
  private:
    Context& context;
    SemaContext& sema;

    TypeResolver resolver;
    TypeBuilder builder;
    FacadeResolver facades;
    DeclarationChecker decl_checker;
    const Type* target_type = nullptr;
    ClosureExpression* active_closure = nullptr;
    Scope* active_closure_scope = nullptr;
    ClosureExpression* direct_closure_callee = nullptr;

    void record_capture(IdentifierExpression& node, const VariableSymbol* symbol) {
        if (active_closure == nullptr || active_closure_scope == nullptr || symbol == nullptr) {
            return;
        }

        for (auto* scope = sema.env.current_scope; scope != nullptr; scope = scope->parent) {
            if (scope->find_local_var(node.name) == symbol) {
                if (scope == active_closure_scope || scope->is_global()) {
                    return;
                }

                if (std::ranges::find(active_closure->captures, node.name) ==
                    active_closure->captures.end()) {
                    active_closure->captures.push_back(node.name);
                }
                return;
            }
        }
    }

    bool is_mutable_place(const Expression& expression) const {
        if (const auto* identifier = expression.as<IdentifierExpression>(); identifier != nullptr) {
            if (identifier->implicit_self_field) {
                const auto* self = sema.env.current_scope->lookup_var("self");
                const auto* pointer = self != nullptr && self->type != nullptr
                                          ? self->type->as<PointerType>()
                                          : nullptr;
                return pointer != nullptr && pointer->is_mutable;
            }

            const auto* symbol = identifier->var_symbol != nullptr
                                     ? identifier->var_symbol
                                     : sema.env.current_scope->lookup_var(identifier->name);
            return symbol != nullptr && symbol->storage_kind == StorageKind::Type::VarMut;
        }

        if (const auto* unary = expression.as<UnaryExpression>();
            unary != nullptr && unary->op == UnaryExpression::Operator::Type::Dereference) {
            const auto* pointer =
                unary->operand->type != nullptr ? unary->operand->type->as<PointerType>() : nullptr;
            return pointer != nullptr && pointer->is_mutable;
        }

        if (const auto* member = expression.as<MemberExpression>(); member != nullptr) {
            return is_mutable_place(*member->value);
        }

        if (const auto* index = expression.as<IndexExpression>(); index != nullptr) {
            const auto* pointer =
                index->value->type != nullptr ? index->value->type->as<PointerType>() : nullptr;
            return pointer != nullptr ? pointer->is_mutable : is_mutable_place(*index->value);
        }

        return false;
    }

    Evaluator create_evaluator() {
        return Evaluator(context.diagnostics, &sema.env, &resolver.environment(), true);
    }

    void define_scope_var(const std::string& name, Span span, Visibility::Type visibility,
                          StorageKind::Type storage_kind, const Type* type) {
        if (!sema.env.current_scope->has_local_var(name)) {
            sema.env.current_scope->define_var(
                name, sema.env.symbols.create<VariableSymbol>(name, span, visibility, storage_kind,
                                                              type));
        }
    }

    const Type* materialize_type(TypeExpression& node) {
        const auto* base = node.source_type;
        for (std::size_t index = 0; index < node.array_sizes.size(); ++index) {
            base = base->as<ArrayType>()->element;
        }

        if (node.element != nullptr) {
            visit(*node.element);
            base = sema.types.create<PointerType>(node.element->type,
                                                  base->as<PointerType>()->is_mutable);
        } else if (node.return_type != nullptr) {
            std::vector<ParameterType> parameters;
            parameters.reserve(node.parameters.size());
            for (auto* parameter : node.parameters) {
                visit(*parameter);
                parameters.emplace_back("", parameter->type);
            }
            visit(*node.return_type);
            base =
                sema.types.create<FunctionType>("", node.return_type->type, std::move(parameters),
                                                std::vector<GenericParameterType>{}, false);
        } else if (!node.generic_arguments.empty()) {
            const auto* receiver = resolver.resolve_type(base);
            GenericArgumentResolver arguments(context, sema, resolver, *this);
            auto resolved = arguments.resolve_type_arguments(node.generic_arguments,
                                                             receiver->as_nominal(), node.span);
            if (!resolved.has_value()) {
                return base;
            }
            base = sema.types.create<NamedType>(base->as<NamedType>()->name, std::move(*resolved));
        }

        auto* type = resolver.resolve_type(base);
        for (auto* expression : node.array_sizes) {
            if (expression == nullptr) {
                type = sema.types.create<ArrayType>(type, UnsizedArrayExtent());
                continue;
            }

            visit_expression(*expression);
            Evaluator evaluator(context.diagnostics, &sema.env, &resolver.environment(), true);
            const auto value = evaluator.evaluate_uncached(*expression);
            if (!value.has_value()) {
                const auto* identifier = expression->as<IdentifierExpression>();
                type = sema.types.create<ArrayType>(
                    type, DependentArrayExtent(expression, identifier != nullptr
                                                               ? identifier->generic_declaration
                                                               : nullptr));
                continue;
            }

            const auto size = value->try_as_unsigned_integer();
            if (!size.has_value()) {
                context.diagnostics.add_error(expression->span,
                                              "array size must be a non-negative constant integer");
                type = sema.types.create<ArrayType>(type, UnsizedArrayExtent());
                continue;
            }

            type = sema.types.create<ArrayType>(
                type, ConcreteArrayExtent(static_cast<std::size_t>(*size)));
        }

        node.type = type;
        return type;
    }

    void visit_expression_with_target(Expression& node, const Type* next_target_type) {
        const auto* saved_target_type = target_type;
        target_type = next_target_type;
        visit_expression(node);
        target_type = saved_target_type;
    }

    bool try_resolve_implicit_self_field(IdentifierExpression& node) {
        if (decl_checker.current_parent.empty() || decl_checker.current_function == nullptr) {
            return false;
        }

        const auto* function_type = decl_checker.current_function->function_type;
        if (function_type == nullptr || function_type->parameters.empty() ||
            function_type->parameters.front().name != "self") {
            return false;
        }

        const auto* parent_symbol =
            sema.env.current_scope->lookup_type(decl_checker.current_parent);
        const auto* parent_type = parent_symbol != nullptr ? parent_symbol->type : nullptr;
        const auto* struct_type = parent_type != nullptr ? parent_type->as<StructType>() : nullptr;
        const auto* field = struct_type != nullptr ? struct_type->find_field(node.name) : nullptr;

        if (field == nullptr) {
            return false;
        }

        node.implicit_self_field = true;
        node.type = resolver.resolve_type(field->type);
        return true;
    }

  public:
    using Visitor<void>::visit;

    void visit(TypeExpression& node) override { node.type = materialize_type(node); }

    void visit(Attribute& node) override {
        const auto* previous = target_type;
        target_type = nullptr;
        for (auto* argument : node.arguments) {
            visit_expression(*argument);
        }
        target_type = previous;
    }

    void visit(GenericParameter& node) override {
        if (node.is_const()) {
            if (node.value_type != nullptr) {
                visit(*node.value_type);
            }
            return;
        }

        if (node.constraint != nullptr) {
            visit(*node.constraint);
        }
    }

    void visit(GenericArgument& node) override {
        if (node.value != nullptr) {
            visit_expression(*node.value);
            return;
        }

        if (node.type != nullptr) {
            visit(*node.type);
        }
    }

    void visit(Parameter& node) override {
        if (node.type != nullptr) {
            visit(*node.type);
        }
    }

    void visit(Argument& node) override { visit_expression(*node.value); }

    void visit(FunctionPrototype& node) override {
        for (auto* generic_parameter : node.generic_parameters) {
            visit(*generic_parameter);
        }

        for (auto* parameter : node.parameters) {
            visit(*parameter);
        }

        if (node.return_type != nullptr) {
            visit(*node.return_type);
        }
    }

    void visit(Field& node) override {
        for (auto* attribute : node.attributes) {
            visit(*attribute);
        }

        if (node.type != nullptr) {
            visit(*node.type);
        }

        if (node.default_value != nullptr) {
            visit_expression(*node.default_value);
        }
    }

    void visit(EnumVariant& node) override {
        for (auto* field : node.fields) {
            visit(*field);
        }

        if (node.value_expression != nullptr) {
            visit_expression(*node.value_expression);
        }
    }

    void visit(StructLiteralField& node) override { visit_expression(*node.value); }

    void visit(WhenPatternField& node [[maybe_unused]]) override {}

    void visit(WhenPattern& node) override {
        if (node.enum_name != nullptr) {
            visit(*node.enum_name);
        }

        if (node.expression != nullptr) {
            visit_expression(*node.expression);
        }
    }

    void visit(WhenArm& node) override {
        for (auto* pattern : node.patterns) {
            visit(*pattern);
        }

        if (node.guard != nullptr) {
            visit_expression(*node.guard);
        }

        if (node.body != nullptr) {
            visit_statement(*node.body);
        }
    }

    void visit(NumberLiteral& node) override {
        if (target_type != nullptr && target_type->is_numeric()) {
            node.type = target_type;
        } else {
            node.type = sema.builtin_resolver.primitives.at("i32");
        }
    }

    void visit(FloatLiteral& node) override {
        if (target_type != nullptr && target_type->is<FloatType>()) {
            node.type = target_type;
        } else {
            node.type = sema.builtin_resolver.primitives.at("f64");
        }
    }

    void visit(StringLiteral& node) override {
        node.type = sema.builtin_resolver.primitives.at("cstr");
    }

    void visit(CharLiteral& node) override {
        node.type = sema.builtin_resolver.primitives.at("char");
    }

    void visit(BooleanLiteral& node) override {
        node.type = sema.builtin_resolver.primitives.at("boolean");
    }

    void visit(NullLiteral& node) override {
        if (target_type != nullptr && target_type->is<PointerType>()) {
            node.type = target_type;
        }
    }

    void visit(IdentifierExpression& node) override {
        const auto* var_symbol = sema.env.current_scope->lookup_var(node.name);
        if (var_symbol != nullptr) {
            node.var_symbol = var_symbol;
            node.type = var_symbol->type;
            record_capture(node, var_symbol);
            return;
        }

        if (try_resolve_implicit_self_field(node)) {
            return;
        }

        const auto* type_symbol = sema.env.current_scope->lookup_type(node.name);
        if (type_symbol != nullptr) {
            node.type = type_symbol->type;
            if (!node.generic_arguments.empty()) {
                GenericArgumentResolver arguments(context, sema, resolver, *this);
                auto bindings = arguments.resolve_type_arguments(
                    node.generic_arguments, node.type->as_nominal(), node.span);
                if (bindings.has_value()) {
                    node.type = resolver.resolve_type(
                        sema.types.create<NamedType>(node.name, std::move(*bindings)));
                }
            }
            return;
        }

        if (resolver.has_type_parameter(node.name)) {
            const auto binding = resolver.const_parameter(node.name);
            if (binding.has_value()) {
                node.type = binding->type;
                node.generic_declaration = resolver.generic_parameter_declaration(node.name);
                if (binding->value.has_value()) {
                    node.compile_time_value = *binding->value;
                }
                return;
            }
            node.type = resolver.resolve_type(
                sema.types.create<NamedType>(node.name, std::vector<GenericArgumentType>{}));
            return;
        }

        const auto& overloads = sema.env.current_scope->lookup_function_overloads(node.name);
        if (!overloads.empty()) {
            node.type = overloads.front()->type;
            if (overloads.size() == 1) {
                node.function_symbol = overloads.front();
                if (!node.generic_arguments.empty()) {
                    const auto& parameters =
                        node.function_symbol->function_type->generic_parameters;
                    GenericArgumentResolver arguments(context, sema, resolver, *this);
                    auto bindings = arguments.resolve_function_arguments(node.generic_arguments,
                                                                         parameters, node.span);
                    if (bindings.has_value()) {
                        auto substitution = resolver.create_substitution_scope();
                        for (std::size_t index = 0; index < bindings->size(); ++index) {
                            resolver.bind_generic_binding(parameters[index].name,
                                                          (*bindings)[index],
                                                          parameters[index].declaration);
                        }
                        node.type = resolver.resolve_type(node.type);
                    }
                }
            } else if (!node.generic_arguments.empty()) {
                context.diagnostics.add_error(
                    node.span, "generic function metadata requires a single overload");
            }
            return;
        }

        context.diagnostics.add_error(node.span, "use of undeclared symbol '" + node.name + "'");
    }

    void visit(QualifiedAccessExpression& node) override {
        MemberResolver member_resolver(context, sema, resolver, facades,
                                       decl_checker.current_parent);
        member_resolver.resolve_qualified(node);
    }

    void visit(BinaryExpression& node) override {
        auto expression_target = target_type;
        if (node.op == BinaryExpression::Operator::Type::As) {
            target_type = nullptr;
            visit_expression(*node.right);

            const auto* cast_type = node.right->type;
            const auto* integer = cast_type != nullptr ? cast_type->as<IntegerType>() : nullptr;
            target_type = integer != nullptr && integer->is_unsigned && integer->size == 64
                              ? cast_type
                              : sema.builtin_resolver.primitives.at("i64");
            visit_expression(*node.left);

            target_type = expression_target;
        } else {
            visit_expression(*node.left);

            auto saved_target_type = target_type;
            target_type = node.left->type;
            visit_expression(*node.right);
            target_type = saved_target_type;
        }

        OperatorResolver operator_resolver(context, sema, resolver);
        operator_resolver.resolve(node);
        target_type = expression_target;
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

            node.type = sema.types.create<BooleanType>();
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
            node.type = sema.types.create<PointerType>(operand_type, false);
            break;
        }
        case Op::AddressOfMut: {
            if (!is_mutable_place(*node.operand)) {
                context.diagnostics.add_error(node.span,
                                              "cannot take mutable reference to immutable value");
                return;
            }

            node.type = sema.types.create<PointerType>(operand_type, true);
            break;
        }
        }
    }

    void visit(CoerceExpression& node) override {
        visit_expression(*node.value);
        node.source_type = node.value->type;
    }

    void visit(BuiltinCall& node) override {
        if (node.type_argument != nullptr) {
            visit(*node.type_argument);
        }

        for (auto* argument : node.arguments) {
            visit_expression(*argument);
        }

        node.type = sema.builtin_resolver.check(node.name, node, context, resolver);
    }

    void visit(CallExpression& node) override {
        auto* saved_direct_closure_callee = direct_closure_callee;
        direct_closure_callee = node.callee->as<ClosureExpression>();
        CallResolver call_resolver(context, sema, resolver, *this, decl_checker.current_parent,
                                   target_type);
        call_resolver.resolve(node);
        direct_closure_callee = saved_direct_closure_callee;
    }

    void visit(IndexExpression& node) override {
        visit_expression(*node.value);
        visit_expression(*node.index);

        const auto* value_type = node.value->type;
        const auto* index_type = node.index->type;
        if (value_type == nullptr || index_type == nullptr) {
            return;
        }

        const auto* integer_type = index_type->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span, "index must be an integer, got '" +
                                                                index_type->to_string() + "'");
            return;
        }

        if (const auto* pointer_type = value_type->as<PointerType>(); pointer_type != nullptr) {
            node.type = pointer_type->element;
            return;
        }

        if (const auto* array_type = value_type->as<ArrayType>(); array_type != nullptr) {
            node.type = array_type->element;
            return;
        }

        if (value_type->is<StringType>()) {
            node.type = sema.builtin_resolver.primitives.at("char");
            return;
        }

        context.diagnostics.add_error(node.span,
                                      "cannot index type '" + value_type->to_string() + "'");
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        MemberResolver member_resolver(context, sema, resolver, facades,
                                       decl_checker.current_parent);
        member_resolver.resolve_member(node);
    }

    void visit(AssignExpression& node) override {
        visit_expression(*node.target);

        const auto* assigned_type = node.target->type;

        visit_expression_with_target(*node.value, assigned_type);

        const auto* value_type = node.value->type;
        if (assigned_type == nullptr || value_type == nullptr) {
            return;
        }

        if (!is_mutable_place(*node.target)) {
            context.diagnostics.add_error(node.target->span, "cannot assign to immutable target");
        }

        if (!facades.accepts(assigned_type, value_type)) {
            context.diagnostics.add_error(node.span, "type mismatch in assignment: expected '" +
                                                         assigned_type->to_string() + "', got '" +
                                                         value_type->to_string() + "'");
        }

        node.value = TypeCoercion::apply(sema, resolver, node.value, assigned_type);
        node.type = assigned_type;
    }

    void visit(StructLiteralExpression& node) override {
        if (auto* qualified = node.name->as<QualifiedAccessExpression>()) {
            const auto* type_symbol = sema.env.current_scope->lookup_type(qualified->parent);
            if (type_symbol != nullptr && type_symbol->type->is<EnumType>()) {
                auto* enum_name =
                    sema.nodes.create<IdentifierExpression>(qualified->span, qualified->parent);
                EnumVariantExpression enum_variant(
                    qualified->span, enum_name, qualified->parent_generic_arguments,
                    qualified->member, !node.fields.empty(), node.fields);
                LiteralResolver literal_resolver(context, sema, resolver, *this, target_type);
                literal_resolver.resolve_enum_literal(enum_variant);

                node.type = enum_variant.type;
                qualified->enum_type = enum_variant.enum_type;
                qualified->variant_type = enum_variant.variant_type;
                return;
            }
        }

        LiteralResolver(context, sema, resolver, *this, target_type)
            .resolve_struct_literal(node, decl_checker.current_parent);
    }

    void visit(EnumVariantExpression& node) override {
        LiteralResolver literal_resolver(context, sema, resolver, *this, target_type);
        literal_resolver.resolve_enum_literal(node);
    }

    void visit(BlockStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                         "block");

        const Type* last_type = nullptr;
        for (auto* statement : node.statements) {
            visit_statement(*statement);
            last_type = statement->type;
        }

        node.type =
            (last_type == nullptr) ? sema.builtin_resolver.primitives.at("void") : last_type;
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

            if (!facades.accepts(then_type, else_type)) {
                context.diagnostics.add_error(
                    node.span, "if/else branches have different types: '" + then_type->to_string() +
                                   "' and '" + else_type->to_string() + "'");
            }

            node.type = then_type;
        } else {
            node.type = sema.builtin_resolver.primitives.at("void");
        }
    }

    void visit(WhileStatement& node) override {
        visit_expression(*node.condition);

        const auto* condition_type = node.condition->type;
        if (condition_type != nullptr && !condition_type->is<BooleanType>()) {
            context.diagnostics.add_error(node.condition->span,
                                          "while condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit(*node.body);

        node.type = sema.builtin_resolver.primitives.at("void");
    }

    void visit(ForStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block, "for");

        visit_statement(*node.initializer);
        visit_expression(*node.condition);

        const auto* condition_type = node.condition->type;
        if (condition_type != nullptr && !condition_type->is<BooleanType>()) {
            context.diagnostics.add_error(node.condition->span,
                                          "for condition must be boolean, got '" +
                                              condition_type->to_string() + "'");
        }

        visit_expression(*node.step);

        visit(*node.body);

        node.type = sema.builtin_resolver.primitives.at("void");
    }

    void visit(DeferStatement& node) override {
        visit_statement(*node.body);
        node.type = sema.builtin_resolver.primitives.at("void");
    }

    void visit(WhenExpression& node) override {
        WhenResolver when_resolver(context, sema, resolver, *this);
        when_resolver.resolve(node);
    }

    void visit(ReturnStatement& node) override {
        if (decl_checker.current_function == nullptr) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
            return;
        }

        const auto* return_type =
            resolver.resolve_type(decl_checker.current_function->function_type->return_type);
        if (return_type == nullptr) {
            return;
        }

        if (node.value != nullptr) {
            visit_expression_with_target(*node.value, return_type);

            const auto* value_type = node.value->type;
            if (value_type == nullptr) {
                context.diagnostics.add_error(node.span, "return value has no type");
                return;
            }

            if (!facades.accepts(return_type, value_type)) {
                context.diagnostics.add_error(node.span, "return type mismatch: expected '" +
                                                             return_type->to_string() + "', got '" +
                                                             value_type->to_string() + "'");
            }

            node.value = TypeCoercion::apply(sema, resolver, node.value, return_type);
        } else {
            if (!return_type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "non-void function must return a value");
            }
        }

        node.type = return_type;
    }

    void visit(InterfaceDeclaration& node) override { decl_checker.declare_interface_type(node); }

    void visit(StructDeclaration& node) override {
        decl_checker.declare_struct_type(node);

        auto saved_parent = decl_checker.current_parent;
        decl_checker.current_parent = node.name;

        for (auto* method : node.methods) {
            visit(*method);
        }

        decl_checker.current_parent = saved_parent;
    }

    void visit(EnumDeclaration& node) override {
        decl_checker.declare_enum_type(node);

        auto saved_parent = decl_checker.current_parent;
        decl_checker.current_parent = node.name;

        for (auto* method : node.methods) {
            visit(*method);
        }

        decl_checker.current_parent = saved_parent;
    }

    void visit(TypeAliasDeclaration& node) override { decl_checker.declare_type_alias(node); }

    void visit(VarDeclaration& node) override {
        for (auto* attribute : node.attributes) {
            visit(*attribute);
        }

        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        const Type* var_type = nullptr;
        if (node.annotation != nullptr) {
            var_type = resolver.resolve_type(node.annotation->type);
        }

        if (var_type != nullptr && !sema.env.current_scope->has_local_var(node.name)) {
            define_scope_var(node.name, node.span, node.visibility, node.storage_kind, var_type);
        }

        if (node.initializer != nullptr) {
            visit_expression_with_target(*node.initializer, var_type);

            if (var_type == nullptr) {
                var_type = resolver.resolve_type(node.initializer->type);
            } else {
                const auto* init_type = node.initializer->type;
                if (init_type != nullptr && !facades.accepts(var_type, init_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in variable declaration: declared '" +
                                       var_type->to_string() + "', initializer is '" +
                                       init_type->to_string() + "'");
                }

                node.initializer = TypeCoercion::apply(sema, resolver, node.initializer, var_type);
            }
        }

        if (var_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot determine type of variable '" + node.name + "'");
        } else if (!sema.env.current_scope->has_local_var(node.name)) {
            define_scope_var(node.name, node.span, node.visibility, node.storage_kind, var_type);
        }

        if (node.initializer != nullptr &&
            (sema.env.current_scope->is_global() ||
             sema.env.current_scope->kind == Scope::Kind::Type::Module) &&
            find_attribute(node.attributes, "section") == nullptr) {
            auto evaluator = create_evaluator();
            evaluator.evaluate(*node.initializer);
        }

        node.type = var_type;
    }

    void visit(FunctionDeclaration& node) override {
        if (!node.is_member() && !sema.env.current_scope->is_global() &&
            sema.env.current_scope->kind != Scope::Kind::Type::Module) {
            context.diagnostics.add_error(node.span,
                                          "functions can only be declared in global scope");
            return;
        }

        if (node.is_extension && !sema.env.current_scope->is_global() &&
            sema.env.current_scope->kind != Scope::Kind::Type::Module) {
            context.diagnostics.add_error(node.span,
                                          "extension methods can only be declared in global scope");
            return;
        }

        if (node.is_extension && sema.env.current_scope->lookup_type(node.parent) == nullptr) {
            context.diagnostics.add_error(node.span, "use of undeclared extension target '" +
                                                         node.parent + "'");
            return;
        }

        if (node.prototype->is_variadic) {
            context.diagnostics.add_error(
                node.span, "variadic parameters are only allowed in extern functions");
            return;
        }

        auto* symbol = decl_checker.declare_function(node);
        if (symbol == nullptr) {
            return;
        }

        if (!decl_checker.validate_function_declaration(node, symbol)) {
            return;
        }

        auto saved_parent = decl_checker.current_parent;
        decl_checker.current_function = symbol;
        decl_checker.current_parent = node.parent;

        auto substitution_scope = resolver.create_substitution_scope();
        if (node.is_member() || node.is_extension) {
            const auto* parent_symbol = sema.env.current_scope->lookup_type(node.parent);
            if (parent_symbol != nullptr) {
                const auto* nominal = parent_symbol->type->as_nominal();
                if (nominal != nullptr) {
                    resolver.bind_generic_parameters(nominal->generic_parameters, false);
                }
                resolver.bind_type_parameter("Self", parent_symbol->type);
            }
        }
        resolver.bind_generic_parameters(symbol->function_type->generic_parameters, false);

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         node.prototype->name);

        for (const auto& parameter : symbol->function_type->parameters) {
            const auto* parameter_type = resolver.resolve_type(parameter.type);
            if (parameter_type == nullptr) {
                context.diagnostics.add_error(node.span, "cannot determine type of parameter '" +
                                                             parameter.name + "'");
            }

            sema.env.current_scope->define_var(
                parameter.name, sema.env.symbols.create<VariableSymbol>(
                                    parameter.name, node.span, Visibility::Type::Private,
                                    StorageKind::Type::Var, parameter_type));
        }

        visit(*node.body);

        decl_checker.current_function = nullptr;
        decl_checker.current_parent = saved_parent;
    }

    void visit(ExternFunctionDeclaration& node) override {
        decl_checker.declare_extern_function(node);
    }

    void visit(ExternVarDeclaration& node) override {
        for (auto* attribute : node.attributes) {
            visit(*attribute);
        }

        decl_checker.declare_extern_variable(node);
    }

    void visit(ImportStatement& node [[maybe_unused]]) override {}

    void visit(ClosureExpression& node) override {
        const auto* target_function_type =
            target_type != nullptr ? target_type->as<FunctionType>() : nullptr;

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         "closure");

        auto* saved_active_closure = active_closure;
        auto* saved_active_closure_scope = active_closure_scope;
        active_closure = &node;
        active_closure_scope = sema.env.current_scope;

        auto parameter_types = builder.build_parameter_types(node.parameters, target_function_type);
        for (std::size_t index = 0; index < parameter_types.size(); ++index) {
            if (parameter_types[index].type == nullptr) {
                context.diagnostics.add_error(node.span,
                                              "cannot infer type of closure parameter '" +
                                                  parameter_types[index].name + "'");
                return;
            }
        }

        for (const auto& parameter_type : parameter_types) {
            sema.env.current_scope->define_var(
                parameter_type.name, sema.env.symbols.create<VariableSymbol>(
                                         parameter_type.name, node.span, Visibility::Type::Private,
                                         StorageKind::Type::Var, parameter_type.type));
        }

        visit(*node.body);

        active_closure = saved_active_closure;
        active_closure_scope = saved_active_closure_scope;

        const Type* return_type = node.body->type;
        if (return_type == nullptr) {
            return_type = sema.builtin_resolver.primitives.at("void");
        }

        const auto* closure_type =
            sema.types.create<FunctionType>("", return_type, std::move(parameter_types),
                                            std::vector<GenericParameterType>{}, false);

        node.type = closure_type;

        if (!node.captures.empty() && direct_closure_callee != &node) {
            context.diagnostics.add_error(node.span,
                                          "captured closures may only be invoked directly");
        }
    }

    void visit(BlockExpression& node) override {
        visit(*node.body);
        node.type = node.body->type;
    }

    void visit(ArrayLiteralExpression& node) override {
        const auto* array_type =
            target_type != nullptr ? resolver.resolve_type(target_type)->as<ArrayType>() : nullptr;
        if (array_type == nullptr) {
            context.diagnostics.add_error(node.span, "array literal requires an array target type");
            return;
        }

        if (!std::holds_alternative<ConcreteArrayExtent>(array_type->extent)) {
            context.diagnostics.add_error(node.span,
                                          "array literal requires a fixed-size array target type");
            return;
        }

        auto expected_size = std::get<ConcreteArrayExtent>(array_type->extent).value;
        if (node.elements.size() != expected_size) {
            context.diagnostics.add_error(
                node.span, "array literal expects " + std::to_string(expected_size) +
                               " elements, got " + std::to_string(node.elements.size()));
            return;
        }

        for (auto* element : node.elements) {
            auto saved_target = target_type;
            target_type = array_type->element;
            visit_expression(*element);
            target_type = saved_target;
        }

        node.type = target_type;
    }

    explicit SemanticChecker(Context& context, SemaContext& sema,
                             const CompileTimeEnvironment* environment = nullptr)
        : context(context), sema(sema), resolver(sema.types, sema.env, context.diagnostics),
          builder(context, sema, resolver, *this), facades(sema, resolver),
          decl_checker(context, sema, resolver, builder, *this,
                       [this](Expression& expression, const Type* target) {
                           visit_expression_with_target(expression, target);
                       }) {
        if (environment != nullptr) {
            resolver.use_environment(*environment);
        }
    }

    void check(Program& program) {
        decl_checker.declare_program_symbols(program);

        for (auto* statement : program.statements) {
            visit_statement(*statement);
        }
    }
};

export using TypeChecker = SemanticChecker;
