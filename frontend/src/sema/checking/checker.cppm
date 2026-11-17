module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.frontend.sema.checking.checker;

import zep.frontend.sema.types;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.common.source.span;
import zep.common.diagnostic.diagnostic;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.declaration;
import zep.frontend.sema.resolution.callable;
import zep.frontend.sema.expression.generic;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.expression.literal;
import zep.frontend.sema.expression.operator_checker;
import zep.frontend.sema.expression.when;
import zep.common.context;
import zep.frontend.sema.context;
import zep.frontend.sema.scopes.environment;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.kind;
import zep.frontend.sema.generics.environment;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.sema.expression.builtin;
import zep.common.state;
import zep.frontend.sema.expression.values;
import zep.frontend.sema.lifecycle;
import zep.frontend.sema.checking.errors;
import zep.frontend.sema.types.errors;

export class SemanticChecker : public Visitor<void> {
  private:
    Context& context;
    SemaContext& sema;

    TypeResolver resolver;
    DeclarationTypeBuilder builder;
    Facades facades;
    Compatibility compatibility;
    DeclarationChecker declarations;

    std::unordered_set<const VariableSymbol*> destroyed;

    const FunctionSymbol* current_function = nullptr;
    const Type* return_type = nullptr;
    const NominalType* enclosing_type = nullptr;
    const Type* target_type = nullptr;
    ClosureExpression* active_closure = nullptr;
    Scope* active_closure_scope = nullptr;
    ClosureExpression* direct_closure_callee = nullptr;
    bool checking_assignment_target = false;

    bool check_condition(Expression& expression, const std::string& statement) {
        visit_expression(expression);
        const auto* type = expression.type;

        if (type == nullptr) {
            return false;
        }

        if (!type->is<BooleanType>()) {
            context.diagnostics.add_error(expression.span,
                statement + " condition must be boolean, got '" + type->to_string() + "'");
        }

        return true;
    }

    void merge_destroyed(const std::unordered_set<const VariableSymbol*>& branch) {
        destroyed.insert(branch.begin(), branch.end());
    }

    void record_capture(IdentifierExpression& node, const VariableSymbol* symbol) {
        if (active_closure == nullptr || active_closure_scope == nullptr || symbol == nullptr) {
            return;
        }

        for (auto* scope = sema.env.current_scope; scope != nullptr; scope = scope->parent) {
            if (scope->find_local_var(node.name) == symbol) {
                if (scope == active_closure_scope || scope->is_global()) {
                    return;
                }

                if (std::ranges::find(active_closure->captures, symbol) ==
                    active_closure->captures.end()) {
                    active_closure->captures.push_back(symbol);
                }
                return;
            }
        }
    }

    Evaluator create_evaluator() {
        return Evaluator(context.diagnostics, &sema.env, &resolver.substitutions,
                         EvaluationPolicy::Type::Dependent);
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
                                                  base->as<PointerType>()->is_mutable,
                                                  base->as<PointerType>()->reference_kind);
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
                    std::vector<GenericParameterType>{}, false,
                    sema.errors.declarations(node.errors, *this, resolver, context.diagnostics));
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

        const auto* named = base->as<NamedType>();
        auto* type = named != nullptr && named->declaration != nullptr && !resolver.is_concrete(base)
            ? base : resolver.resolve_type(base);
        for (auto* expression : node.array_sizes) {
            if (expression == nullptr) {
                type = sema.types.create<ArrayType>(type, UnsizedArrayExtent());
                continue;
            }

            visit_expression(*expression);
            Evaluator evaluator(context.diagnostics, &sema.env, &resolver.substitutions,
                                EvaluationPolicy::Type::Dependent);
            const auto result = evaluator.evaluate(*expression);

            if (result.state == EvaluationResult::State::Dependent) {
                const auto* identifier = expression->as<IdentifierExpression>();
                type = sema.types.create<ArrayType>(
                    type, DependentArrayExtent(expression, identifier != nullptr
                                                               ? identifier->generic_declaration
                                                               : nullptr));
                continue;
            }

            if (result.state == EvaluationResult::State::Invalid) {
                type = sema.types.create<ArrayType>(type, UnsizedArrayExtent());
                continue;
            }

            const auto size = result.value->try_as_unsigned_integer();
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
        StateGuard target_guard(target_type, next_target_type);
        visit_expression(node);
        sema.binding.reference(node, resolver.resolve_type(next_target_type), sema.env, context.diagnostics);
    }

    bool try_resolve_implicit_self_field(IdentifierExpression& node) {
        if (enclosing_type == nullptr || current_function == nullptr) {
            return false;
        }

        const auto* function_type = current_function->signature();
        if (function_type == nullptr || function_type->parameters.empty() ||
            function_type->parameters.front().name != "self") {
            return false;
        }

        const auto* struct_type = enclosing_type->as<StructType>();
        const auto* enum_type = enclosing_type->as<EnumType>();
        const auto* field = struct_type != nullptr ? struct_type->find_field(node.name)
            : enum_type != nullptr ? enum_type->find_field(node.name) : nullptr;

        if (field == nullptr) {
            return false;
        }

        node.implicit_self_field = true;
        node.type = field->type;
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

            if (node.type->type != nullptr && node.type->type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "stored fields cannot explicitly have type void");
            }
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
        const auto* resolved = resolver.constraint_type(target_type);

        if (resolved != nullptr && resolved->is_numeric()) {
            node.type = target_type;
        } else {
            node.type = sema.primitives.entries.at("i32");
        }
    }

    void visit(FloatLiteral& node) override {
        const auto* resolved = resolver.constraint_type(target_type);

        if (resolved != nullptr && resolved->is<FloatType>()) {
            node.type = target_type;
        } else {
            node.type = sema.primitives.entries.at("f64");
        }
    }

    void visit(StringLiteral& node) override {
        node.type = sema.primitives.entries.at("cstr");
    }

    void visit(CharLiteral& node) override {
        node.type = sema.primitives.entries.at("char");
    }

    void visit(BooleanLiteral& node) override {
        node.type = sema.primitives.entries.at("boolean");
    }

    void visit(NullLiteral& node) override {
        if (target_type != nullptr && target_type->is<PointerType>()) {
            node.type = target_type;
        }
    }

    void visit(IdentifierExpression& node) override {
        const auto* var_symbol = sema.env.current_scope->lookup_var(node.name);
        if (var_symbol != nullptr) {
            if (!checking_assignment_target && destroyed.contains(var_symbol)) {
                context.diagnostics.add_error(node.span, "use of destroyed value '" + node.name +
                                                             "'");
            }
            node.var_symbol = var_symbol;
            node.type = var_symbol->type;
            if (const auto* reference = node.type->as<PointerType>(); reference != nullptr &&
                reference->reference_kind != PointerType::ReferenceKind::Type::None) {
                node.type = reference->element;
            }
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
                node.generic_declaration = binding->declaration;
                if (binding->value.has_value()) {
                    node.compile_time_value = *binding->value;
                }
                return;
            }
            const void* declaration = nullptr;

            if (current_function != nullptr) {
                for (const auto& parameter : current_function->signature()->generic_parameters) {
                    if (parameter.name == node.name) {
                        declaration = parameter.declaration;
                    }
                }
            }

            node.type = sema.types.create<NamedType>(node.name, std::vector<GenericArgumentType>{}, declaration);
            return;
        }

        const auto& overloads = sema.env.current_scope->lookup_function_overloads(node.name);
        if (!overloads.empty()) {
            node.type = overloads.front()->type;
            if (overloads.size() == 1) {
                node.function_symbol = overloads.front();
                if (!node.generic_arguments.empty()) {
                    const auto& parameters =
                        node.function_symbol->signature()->generic_parameters;
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
        LiteralChecker literals(context, sema, resolver, *this, target_type);

        if (literals.resolve_variant(node)) {
            return;
        }

        MemberResolver member_resolver(context, sema, resolver, facades, enclosing_type);
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
                              : sema.primitives.entries.at("i64");
            visit_expression(*node.left);

            target_type = expression_target;
        } else {
            visit_expression(*node.left);

            StateGuard target_guard(target_type, node.left->type);
            visit_expression(*node.right);
        }

        OperatorChecker operator_checker(context, sema, resolver);
        operator_checker.resolve(node);
        target_type = expression_target;
    }

    void visit(UnaryExpression& node) override {
        using Operator = UnaryExpression::Operator::Type;

        visit_expression(*node.operand);

        const auto* operand_type = node.operand->type;
        if (operand_type == nullptr) {
            return;
        }

        switch (node.op) {
        case Operator::Plus:
        case Operator::Minus: {
            if (!resolver.constraint_type(operand_type)->is_numeric()) {
                context.diagnostics.add_error(node.span,
                                              "operand of unary +/- must be numeric, got '" +
                                                  operand_type->to_string() + "'");
                return;
            }

            node.type = operand_type;
            break;
        }
        case Operator::Not: {
            if (!resolver.constraint_type(operand_type)->is<BooleanType>()) {
                context.diagnostics.add_error(node.span, "operand of '!' must be boolean, got '" +
                                                             operand_type->to_string() + "'");
                return;
            }

            node.type = sema.types.create<BooleanType>();
            break;
        }
        case Operator::Dereference: {
            const auto* pointer = operand_type->as<PointerType>();
            if (pointer == nullptr) {
                context.diagnostics.add_error(node.span, "cannot dereference non-pointer type '" +
                                                             operand_type->to_string() + "'");
                return;
            }

            node.type = pointer->element;
            break;
        }
        case Operator::AddressOf:
        case Operator::AddressOfMut: {
            const auto* reference = node.type != nullptr ? node.type->as<PointerType>() : nullptr;
            const auto kind = reference != nullptr ? reference->reference_kind : PointerType::ReferenceKind::Type::None;
            const auto is_mutable = node.op == Operator::AddressOfMut;
            const auto temporary = kind == PointerType::ReferenceKind::Type::Move && !Values::place(*node.operand);

            if (is_mutable && !temporary && !Values::mutable_place(*node.operand, sema.env)) {
                context.diagnostics.add_error(node.span,
                                              "cannot take mutable reference to immutable value");

                return;
            }

            node.type = sema.types.create<PointerType>(operand_type, is_mutable, kind);

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

        node.type = BuiltinChecker(context, resolver, sema.primitives).check(node);

        if (node.type != nullptr && (node.name == "sizeof" || node.name == "length")) {
            node.compile_time_value = create_evaluator().evaluate(node).value;
        }
    }

    void visit(CallExpression& node) override {
        sema.errors.targets.erase(&node);
        if (auto* member = node.callee->as<MemberExpression>();
            member != nullptr && member->member.starts_with("~")) {
            if (!node.arguments.empty() || !node.generic_arguments.empty()) {
                context.diagnostics.add_error(node.span, "destructor calls take no arguments");

                return;
            }

            {
                StateGuard target_guard(checking_assignment_target, true);
                visit_expression(*member->value);
            }

            const auto* type = resolver.resolve_type(member->value->type);

            if (type == nullptr) {
                return;
            }

            if (!Values::mutable_place(*member->value, sema.env)) {
                context.diagnostics.add_error(node.span, "explicit destruction requires a mutable place");

                return;
            }

            if (TypeLayout().resolve(type).state == TypeLayout::Result::State::Invalid) {
                context.diagnostics.add_error(node.span, "explicit destruction requires a sized value");

                return;
            }

            const auto name = member->member.substr(1);
            const auto* expected = sema.primitives.entries.contains(name)
                ? sema.primitives.entries.at(name)
                : resolver.resolve_type(sema.types.create<NamedType>(name, std::vector<GenericArgumentType>()));
            const auto* nominal = type->as_nominal();
            const auto* expected_nominal = expected->as_nominal();

            if (!type->same(expected) &&
                (nominal == nullptr || expected_nominal == nullptr ||
                 nominal->definition != expected_nominal->definition)) {
                context.diagnostics.add_error(node.span, "destructor type does not match the value type");

                return;
            }

            node.destruction = sema.lifecycle.resolve(type, Operation::Type::Destroy, resolver);

            if (node.destruction->action == LifecyclePlan::Action::Type::Deleted) {
                context.diagnostics.add_error(node.span, "use of deleted destructor");

                return;
            }

            if (const auto* function = node.destruction->function; function != nullptr &&
                !MemberResolver::is_visible(function->visibility, enclosing_type, nominal)) {
                context.diagnostics.add_error(node.span, "destructor is not accessible");

                return;
            }

            if (const auto* identifier = member->value->as<IdentifierExpression>();
                identifier != nullptr && identifier->var_symbol != nullptr &&
                !destroyed.insert(identifier->var_symbol).second) {
                context.diagnostics.add_error(node.span, "value '" + identifier->name + "' is already destroyed");
            }

            node.type = sema.primitives.entries.at("void");

            return;
        }

        StateGuard closure_guard(direct_closure_callee,
                                 node.callee->as<ClosureExpression>());
        CallResolver call_resolver(context, sema, resolver, *this, enclosing_type, target_type);
        call_resolver.resolve(node);
        const FunctionType* signature = nullptr;

        if (node.resolved_target != nullptr) {
            if (const auto* direct = node.resolved_target->as<DirectCallTarget>(); direct != nullptr) {
                signature = direct->signature;
            } else if (const auto* indirect = node.resolved_target->as<IndirectCallTarget>(); indirect != nullptr) {
                signature = indirect->function_type;
            } else if (const auto* dispatch = node.resolved_target->as<InterfaceCallTarget>(); dispatch != nullptr) {
                signature = dispatch->signature;
            }
        }

        if (signature != nullptr) {
            for (const auto* error : signature->errors) {
                sema.errors.raise(node, error, Errors::Origin::Type::Call, resolver, context.diagnostics);
            }
        }

        if (const auto* reference = node.type != nullptr ? node.type->as<PointerType>() : nullptr;
            reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            node.reference_result = reference;
            node.type = reference->element;
        }

    }

    void visit(TryExpression& node) override {
        sema.errors.targets.erase(&node);
        sema.errors.protect();
        visit_expression(*node.value);
        const auto escaping = sema.errors.finish();
        node.type = node.value->type;

        for (const auto* error : escaping) {
            sema.errors.raise(node, error, Errors::Origin::Type::Forward, resolver, context.diagnostics);
        }
    }

    void visit(ThrowExpression& node) override {
        sema.errors.targets.erase(&node);
        StateGuard target_guard(target_type, static_cast<const Type*>(nullptr));
        visit_expression(*node.value);
        node.type = sema.primitives.entries.at("never");
        node.value = sema.binding.construct(node.value, node.value->type, resolver, context.diagnostics);
        sema.errors.raise(node, node.value->type, Errors::Origin::Type::Throw, resolver, context.diagnostics);
    }

    void visit(CatchExpression& node) override {
        sema.errors.targets.erase(&node);
        std::vector<const Type*> handlers;
        handlers.reserve(node.arms.size());

        for (auto& arm : node.arms) {
            visit(*arm.annotation);
            const auto* type = resolver.resolve_type(arm.annotation->type);
            arm.annotation->type = type;

            if (type != nullptr && ErrorSet::contains(handlers, type)) {
                context.diagnostics.add_error(arm.span, "duplicate catch handler for '" + type->to_string() + "'");
            }

            if (type != nullptr) {
                handlers.push_back(type);
            }
        }

        sema.errors.protect(&node);
        visit_expression(*node.value);
        const auto failures = sema.errors.finish();
        const auto entry_state = destroyed;
        auto merged_state = destroyed;
        const Type* result = node.value->type;

        for (auto& arm : node.arms) {
            const auto* type = arm.annotation->type;

            if (type == nullptr) {
                continue;
            }

            if (!ErrorSet::contains(failures, type)) {
                context.diagnostics.add_error(arm.span, "catch handler cannot receive '" + type->to_string() + "'");
            }

            destroyed = entry_state;
            ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block, "catch");
            arm.scope = sema.env.current_scope;
            auto* symbol = sema.env.symbols.create<VariableSymbol>(arm.name, arm.span,
                Visibility::Type::Private, StorageKind::Type::VarMut, type);
            arm.symbol = symbol;
            arm.scope->define_var(arm.name, symbol);

            StateGuard target_guard(target_type,
                result != nullptr && !result->is<NeverType>() ? result : target_type);
            visit_statement(*arm.body);

            if (auto* tail = Values::tail(*arm.body); tail != nullptr) {
                tail->expression = sema.binding.initialize(tail->expression,
                    result != nullptr && !result->is<NeverType>() ? result : tail->expression->type,
                    resolver, context.diagnostics);
            }

            merged_state.insert(destroyed.begin(), destroyed.end());
            const auto* branch = arm.body->type;

            if (result == nullptr || result->is<NeverType>()) {
                result = branch;
            } else if (branch != nullptr && !compatibility.accepts(result, branch)) {
                context.diagnostics.add_error(arm.span, "catch handler result does not match the success type");
            }
        }

        destroyed = std::move(merged_state);
        node.type = result;
        for (const auto* error : failures) {
            if (!ErrorSet::contains(handlers, error)) {
                sema.errors.raise(node, error, Errors::Origin::Type::Call, resolver, context.diagnostics);
            }
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

        const auto* integer_type = index_type->as<IntegerType>();
        if (integer_type == nullptr) {
            context.diagnostics.add_error(node.index->span, "index must be an integer, got '" +
                                                                index_type->to_string() + "'");
            return;
        }

        if (const auto* pointer_type = value_type->as<PointerType>(); pointer_type != nullptr) {
            node.type = pointer_type->element;
            if (const auto* reference = node.type->as<PointerType>(); reference != nullptr &&
                reference->reference_kind != PointerType::ReferenceKind::Type::None) {
                node.type = reference->element;
            }
            return;
        }

        if (const auto* array_type = value_type->as<ArrayType>(); array_type != nullptr) {
            node.type = array_type->element;
            if (const auto* reference = node.type->as<PointerType>(); reference != nullptr &&
                reference->reference_kind != PointerType::ReferenceKind::Type::None) {
                node.type = reference->element;
            }
            return;
        }

        if (value_type->is<StringType>()) {
            node.type = sema.primitives.entries.at("char");
            return;
        }

        context.diagnostics.add_error(node.span,
                                      "cannot index type '" + value_type->to_string() + "'");
    }

    void visit(MemberExpression& node) override {
        visit_expression(*node.value);

        MemberResolver member_resolver(context, sema, resolver, facades, enclosing_type);
        member_resolver.resolve_member(node);
        if (const auto* reference = node.type != nullptr ? node.type->as<PointerType>() : nullptr;
            reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            node.type = reference->element;
        }
    }

    void visit(NewExpression& node) override {
        const Type* hint = nullptr;
        const auto* address = node.storage->as<UnaryExpression>();
        const auto* local = address != nullptr && address->op == UnaryOperator::Type::AddressOfMut
            ? address->operand->as<IdentifierExpression>() : nullptr;

        if (node.storage != nullptr) {
            StateGuard storage_guard(checking_assignment_target, local != nullptr);
            visit_expression(*node.storage);
            const auto* type = resolver.resolve_type(node.storage->type);
            const auto* pointer = type != nullptr ? type->as<PointerType>() : nullptr;

            if (pointer == nullptr || !pointer->is_mutable ||
                pointer->reference_kind != PointerType::ReferenceKind::Type::None) {
                context.diagnostics.add_error(node.span, "placement new requires a mutable raw pointer");

                return;
            }

            hint = pointer->element->is<VoidType>() ? nullptr : pointer->element;
        }

        visit_expression_with_target(*node.value, hint);
        const auto* type = resolver.resolve_type(node.value->type);

        if (type == nullptr) {
            return;
        }

        if (TypeLayout().resolve(type).state == TypeLayout::Result::State::Invalid) {
            context.diagnostics.add_error(node.span, "new requires a sized value");

            return;
        }

        if (hint != nullptr && !hint->same(type)) {
            context.diagnostics.add_error(node.span, "placement value does not match the storage type");
            return;
        }

        const auto* stored = hint != nullptr ? hint : type;
        node.value = sema.binding.construct(node.value, stored, resolver, context.diagnostics);
        node.type = sema.types.create<PointerType>(stored, true);

        if (local != nullptr && local->var_symbol != nullptr) {
            destroyed.erase(local->var_symbol);
        }

    }

    void visit(AssignExpression& node) override {
        {
            StateGuard assignment_guard(checking_assignment_target, true);
            visit_expression(*node.target);
        }

        const auto* assigned_type = node.target->type;

        visit_expression_with_target(*node.value, assigned_type);

        const auto* value_type = node.value->type;
        if (assigned_type == nullptr || value_type == nullptr) {
            return;
        }

        if (!Values::mutable_place(*node.target, sema.env)) {
            context.diagnostics.add_error(node.target->span, "cannot assign to immutable target");
        }

        if (!compatibility.accepts(assigned_type, value_type)) {
            context.diagnostics.add_error(node.span, "type mismatch in assignment: expected '" +
                                                         assigned_type->to_string() + "', got '" +
                                                         value_type->to_string() + "'");
        }

        const auto* nominal = resolver.resolve_type(assigned_type)->as_nominal();
        auto* members = Scope::members(nominal);
        if (members != nullptr && !members->lookup_function_overloads("operator=").empty()) {
            auto* member = sema.nodes.create<MemberExpression>(node.span, node.target, "operator=");
            std::vector<Argument*> arguments;
            arguments.reserve(1);
            arguments.push_back(sema.nodes.create<Argument>(node.span, std::string(), node.value));
            auto* call = sema.nodes.create<CallExpression>(node.span, member, std::vector<GenericArgument*>(), std::move(arguments));
            visit(*call);
            node.resolved_assignment = call;
            node.type = assigned_type;
            return;
        }

        node.value = sema.binding.initialize(node.value, assigned_type, resolver, context.diagnostics);
        if (auto* identifier = node.target->as<IdentifierExpression>(); identifier != nullptr &&
            identifier->var_symbol != nullptr) {
            destroyed.erase(identifier->var_symbol);
        }
        node.type = assigned_type;
    }

    void visit(StructLiteralExpression& node) override {
        LiteralChecker literals(context, sema, resolver, *this, target_type);

        if (auto* qualified = node.name->as<QualifiedAccessExpression>(); qualified != nullptr) {
            if (literals.resolve_variant(*qualified, node.fields)) {
                node.type = qualified->type;
                return;
            }
        }

        literals.resolve_struct_literal(node, enclosing_type);
    }

    void visit(EnumVariantExpression& node) override {
        LiteralChecker literal_checker(context, sema, resolver, *this, target_type);
        literal_checker.resolve_enum_literal(node);
    }

    void visit(BlockStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                         "block");
        node.scope = sema.env.current_scope;

        const Type* last_type = nullptr;
        for (auto* statement : node.statements) {
            visit_statement(*statement);
            last_type = statement->type;
        }

        node.type =
            (last_type == nullptr) ? sema.primitives.entries.at("void") : last_type;
    }

    void visit(ExpressionStatement& node) override {
        visit_expression(*node.expression);
        node.type = node.expression->type;
    }

    void visit(IfExpression& node) override {
        if (!check_condition(*node.condition, "if")) {
            return;
        }

        const auto entry_state = destroyed;
        visit_statement(*node.then_branch);
        const auto then_state = destroyed;

        if (node.else_branch == nullptr) {
            merge_destroyed(entry_state);
            node.type = sema.primitives.entries.at("void");
            return;
        }

        destroyed = entry_state;
        visit_statement(*node.else_branch);
        merge_destroyed(then_state);

        const auto* then_type = node.then_branch->type;
        const auto* else_type = node.else_branch->type;

        if (then_type == nullptr || else_type == nullptr) {
            return;
        }

        if (!compatibility.accepts(then_type, else_type)) {
            context.diagnostics.add_error(node.span,
                "if/else branches have different types: '" + then_type->to_string() +
                "' and '" + else_type->to_string() + "'");
        }

        node.type = then_type->is<NeverType>() ? else_type : then_type;
    }

    void visit(WhileStatement& node) override {
        check_condition(*node.condition, "while");

        const auto entry_state = destroyed;
        visit(*node.body);
        merge_destroyed(entry_state);

        node.type = sema.primitives.entries.at("void");
    }

    void visit(ForStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block, "for");
        node.scope = sema.env.current_scope;

        visit_statement(*node.initializer);
        check_condition(*node.condition, "for");

        const auto entry_state = destroyed;
        visit(*node.body);
        visit_expression(*node.step);
        merge_destroyed(entry_state);

        node.type = sema.primitives.entries.at("void");
    }

    void visit(DeferStatement& node) override {
        StateGuard error_guard(sema.errors.state, Errors::State());
        visit_statement(*node.body);
        node.type = sema.primitives.entries.at("void");
    }

    void visit(WhenExpression& node) override {
        WhenChecker when_checker(context, sema, resolver, *this, target_type);
        when_checker.resolve(node);
    }

    void visit(ReturnStatement& node) override {
        if (current_function == nullptr && active_closure == nullptr) {
            context.diagnostics.add_error(node.span, "return statement outside of function");
            return;
        }

        if (node.value != nullptr) {
            visit_expression_with_target(*node.value, return_type);

            if (return_type == nullptr) {
                return_type = node.value->type;
            }

            if (return_type == nullptr) {
                context.diagnostics.add_error(node.span, "return value has no type");
                return;
            }

            const auto* reference = return_type->as<PointerType>();
            const Expression* referent = node.value;
            while (true) {
                if (const auto* member = referent->as<MemberExpression>(); member != nullptr &&
                    member->value->type != nullptr && !member->value->type->is<PointerType>()) {
                    referent = member->value;
                } else if (const auto* index = referent->as<IndexExpression>(); index != nullptr &&
                           index->value->type != nullptr && index->value->type->is<ArrayType>()) {
                    referent = index->value;
                } else {
                    break;
                }
            }
            const auto* identifier = referent->as<IdentifierExpression>();
            if (reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None &&
                identifier != nullptr && identifier->var_symbol != nullptr &&
                !identifier->var_symbol->type->is<PointerType>()) {
                for (auto* scope = sema.env.current_scope; scope != nullptr; scope = scope->parent) {
                    if (scope->find_local_var(identifier->name) == identifier->var_symbol) {
                        if (!scope->is_global() && scope->kind != Scope::Kind::Type::Module) {
                            context.diagnostics.add_error(node.span, "cannot return a reference to a local value");
                        }
                        break;
                    }
                }
            }

            const auto* value_type = node.value->type;
            if (value_type == nullptr) {
                context.diagnostics.add_error(node.span, "return value has no type");
                return;
            }

            if (!compatibility.accepts(return_type, value_type)) {
                context.diagnostics.add_error(node.span, "return type mismatch: expected '" +
                                                             return_type->to_string() + "', got '" +
                                                             value_type->to_string() + "'");
            }

            node.value = sema.binding.initialize(node.value, return_type, resolver, context.diagnostics);
        } else {
            if (return_type == nullptr) {
                return_type = sema.primitives.entries.at("void");
            }

            if (!return_type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "non-void function must return a value");
            }
        }

        node.type = sema.primitives.entries.at("never");
    }

    void visit(InterfaceDeclaration& node) override {
        declarations.nominals.declare_interface_type(node);
    }

    void visit(StructDeclaration& node) override {
        declarations.nominals.declare_struct_type(node);

        StateGuard enclosing_guard(
            enclosing_type, node.type != nullptr ? node.type->as_nominal() : nullptr);

        for (auto* method : node.methods) {
            visit(*method);
        }

    }

    void visit(EnumDeclaration& node) override {
        declarations.nominals.declare_enum_type(node);

        StateGuard enclosing_guard(
            enclosing_type, node.type != nullptr ? node.type->as_nominal() : nullptr);

        for (auto* method : node.methods) {
            visit(*method);
        }

    }

    void visit(TypeAliasDeclaration& node) override {
        declarations.declare_type_alias(node);
    }

    void visit(VarDeclaration& node) override {
        for (auto* attribute : node.attributes) {
            visit(*attribute);
        }

        if (node.annotation != nullptr) {
            visit(*node.annotation);
        }

        const Type* var_type = nullptr;
        if (node.annotation != nullptr) {
            var_type = node.annotation->type;
        }

        if (var_type != nullptr && !sema.env.current_scope->has_local_var(node.name)) {
            define_scope_var(node.name, node.span, node.visibility, node.storage_kind, var_type);
        }

        if (node.initializer != nullptr) {
            visit_expression_with_target(*node.initializer, var_type);

            if (var_type == nullptr) {
                var_type = node.initializer->type;
            } else {
                const auto* init_type = node.initializer->type;
                if (init_type != nullptr && !compatibility.accepts(var_type, init_type)) {
                    context.diagnostics.add_error(
                        node.span, "type mismatch in variable declaration: declared '" +
                                       var_type->to_string() + "', initializer is '" +
                                       init_type->to_string() + "'");
                }

                node.initializer = sema.binding.convert(node.initializer, var_type, resolver);
            }

            node.initializer = sema.binding.construct(node.initializer, var_type, resolver, context.diagnostics);
        }

        if (var_type == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot determine type of variable '" + node.name + "'");
        } else if (var_type->is<VoidType>()) {
            context.diagnostics.add_error(node.span, "variables cannot have type void");
        } else if (!sema.env.current_scope->has_local_var(node.name)) {
            define_scope_var(node.name, node.span, node.visibility, node.storage_kind, var_type);
        }

        if (node.initializer != nullptr &&
            (sema.env.current_scope->is_global() ||
             sema.env.current_scope->kind == Scope::Kind::Type::Module)) {
            Evaluator evaluator(context.diagnostics, &sema.env, &resolver.substitutions,
                                EvaluationPolicy::Type::Initializer);
            node.initializer->compile_time_value = evaluator.evaluate(*node.initializer).value;
        }

        node.type = var_type;

        if (node.variable_symbol != nullptr) {
            node.variable_symbol->type = var_type;
        } else if (sema.env.current_scope->has_local_var(node.name)) {
            node.variable_symbol = sema.env.current_scope->local_variables().at(node.name);
        }
    }

    void visit(TestDeclaration& node) override {
        visit(*node.function);
        node.type = node.function->type;
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

        auto* symbol = declarations.functions.declare_function(node);
        if (symbol == nullptr) {
            return;
        }

        if (!declarations.functions.validate_function_declaration(node, symbol)) {
            return;
        }
        if (node.is_deleted) {
            return;
        }

        StateGuard function_guard(current_function, symbol);
        StateGuard return_guard(return_type, symbol->signature()->return_type);
        StateGuard error_guard(sema.errors.state, Errors::State(symbol->signature()->errors));
        StateGuard lifetime_guard(destroyed, std::unordered_set<const VariableSymbol*>());

        const NominalType* function_owner = nullptr;

        auto substitution_scope = resolver.create_substitution_scope();
        if (node.is_member() || node.is_extension) {
            const auto* parent_symbol = sema.env.current_scope->lookup_type(node.parent);
            if (parent_symbol != nullptr) {
                const auto* nominal = parent_symbol->type->as_nominal();
                function_owner = nominal;
                if (nominal != nullptr) {
                    resolver.bind_generic_parameters(nominal->generic_parameters);
                }
                resolver.bind_type_parameter("Self", parent_symbol->type);
            }
        }

        StateGuard enclosing_guard(enclosing_type, function_owner);
        resolver.bind_generic_parameters(symbol->signature()->generic_parameters);

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         node.prototype->name);
        node.scope = sema.env.current_scope;

        for (const auto& parameter : symbol->signature()->parameters) {
            const auto* parameter_type = resolver.resolve_type(parameter.type);
            if (parameter_type == nullptr) {
                context.diagnostics.add_error(node.span, "cannot determine type of parameter '" +
                                                             parameter.name + "'");
            }

            sema.env.current_scope->define_var(
                parameter.name, sema.env.symbols.create<VariableSymbol>(
                                    parameter.name, node.span, Visibility::Type::Private,
                                    StorageKind::Type::Var, parameter.type));
        }

        if (node.is_defaulted) {
            const auto* structure = function_owner != nullptr ? function_owner->as<StructType>() : nullptr;
            const auto construction = symbol->has_role(ConstructorKind::Type::Copy) ||
                                      symbol->has_role(ConstructorKind::Type::Move);
            const auto assignment = node.prototype->name == "operator=";
            if (structure == nullptr || (!construction && !assignment && !symbol->has_role(CallableKind::Type::Destructor))) {
                context.diagnostics.add_error(node.span, "only special members can be defaulted");
                return;
            }
            if (construction || assignment) {
                if (node.prototype->parameters.size() != 1) {
                    context.diagnostics.add_error(node.span, "defaulted copy/move operations require one source parameter");
                    return;
                }
                auto* source = sema.nodes.create<IdentifierExpression>(node.span, node.prototype->parameters[0]->name);
                const auto* source_type = symbol->signature()->parameters.back().type->as<PointerType>();
                const auto moving = source_type != nullptr && source_type->reference_kind == PointerType::ReferenceKind::Type::Move;
                if (!sema.lifecycle.available(structure,
                    moving ? Operation::Type::Move : Operation::Type::Copy,
                    resolver, LifecycleMode::Type::Defaulted)) {
                    sema.lifecycle.disable(symbol);
                    node.is_deleted = true;
                    return;
                }
                if (construction) {
                    node.is_defaulted = false;
                    return;
                }
                node.body->statements.reserve(structure->fields.size() + 2);
                auto inherited_count = std::size_t{0};
                if (structure->base_type != nullptr) {
                    const auto* base_pointer_type = sema.types.create<PointerType>(structure->base_type, true);
                    auto* self = sema.nodes.create<IdentifierExpression>(node.span, "self");
                    auto* target_pointer = sema.nodes.create<CoerceExpression>(node.span, self, base_pointer_type, Coercion::Type::None, nullptr);
                    auto* target = sema.nodes.create<UnaryExpression>(node.span, UnaryOperator::Type::Dereference, target_pointer);
                    auto* source_address = sema.nodes.create<UnaryExpression>(node.span,
                        moving ? UnaryOperator::Type::AddressOfMut : UnaryOperator::Type::AddressOf, source);
                    auto* source_pointer = sema.nodes.create<CoerceExpression>(node.span, source_address,
                        sema.types.create<PointerType>(structure->base_type, moving), Coercion::Type::None, nullptr);
                    Expression* value = sema.nodes.create<UnaryExpression>(node.span, UnaryOperator::Type::Dereference, source_pointer);
                    if (moving) {
                        value = sema.binding.construct(value, structure->base_type, resolver,
                                                       context.diagnostics, Operation::Type::Move);
                    }
                    auto* operation = sema.nodes.create<AssignExpression>(node.span, target, value);
                    node.body->statements.push_back(sema.nodes.create<ExpressionStatement>(operation));
                    inherited_count = structure->base_type->fields.size();
                }
                for (auto index = inherited_count; index < structure->fields.size(); ++index) {
                    const auto& field = structure->fields[index];
                    Expression* value = sema.nodes.create<MemberExpression>(node.span, source, field.name);
                    if (moving) {
                        value = sema.binding.construct(value, field.type, resolver,
                                                       context.diagnostics, Operation::Type::Move);
                    }

                    auto* self = sema.nodes.create<IdentifierExpression>(node.span, "self");
                    auto* object = sema.nodes.create<UnaryExpression>(node.span, UnaryOperator::Type::Dereference, self);
                    auto* target = sema.nodes.create<MemberExpression>(node.span, object, field.name);
                    auto* expression = sema.nodes.create<AssignExpression>(node.span, target, value);
                    node.body->statements.push_back(sema.nodes.create<ExpressionStatement>(expression));
                }

                auto* self = sema.nodes.create<IdentifierExpression>(node.span, "self");
                auto* result = sema.nodes.create<UnaryExpression>(node.span, UnaryOperator::Type::Dereference, self);
                node.body->statements.push_back(sema.nodes.create<ReturnStatement>(node.span, result));
            }
            node.is_defaulted = false;
        }

        visit(*node.body);

    }

    void visit(ExternFunctionDeclaration& node) override {
        declarations.functions.declare_extern_function(node);
    }

    void visit(ExternVarDeclaration& node) override {
        for (auto* attribute : node.attributes) {
            visit(*attribute);
        }

        declarations.declare_variable(node);
    }

    void visit(ImportStatement& node [[maybe_unused]]) override {}

    void visit(ClosureExpression& node) override {
        const auto* target_function_type =
            target_type != nullptr ? target_type->as<FunctionType>() : nullptr;

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         "closure");

        StateGuard closure_guard(active_closure, &node);
        StateGuard closure_scope_guard(active_closure_scope, sema.env.current_scope);
        StateGuard return_guard(return_type,
            target_function_type != nullptr ? target_function_type->return_type : nullptr);
        StateGuard target_guard(target_type, return_type);
        const auto declared = node.declares_errors
            ? sema.errors.declarations(node.errors, *this, resolver, context.diagnostics)
            : target_function_type != nullptr ? target_function_type->errors : std::vector<const Type*>();
        StateGuard error_guard(sema.errors.state, Errors::State(declared,
            !node.declares_errors && target_function_type == nullptr));

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

        if (return_type != nullptr) {
            if (!compatibility.accepts(return_type, node.body->type)) {
                context.diagnostics.add_error(node.span, "closure return type mismatch: expected '" +
                    return_type->to_string() + "', got '" + node.body->type->to_string() + "'");
            }
        } else {
            return_type = node.body->type != nullptr
                ? node.body->type : sema.primitives.entries.at("void");
        }

        const auto* closure_type =
            sema.types.create<FunctionType>("", return_type, std::move(parameter_types),
                std::vector<GenericParameterType>{}, false, sema.errors.state.inferred
                    ? ErrorSet::normalize(sema.errors.state.escaped) : declared);

        node.type = closure_type;

        if (!node.captures.empty() && direct_closure_callee != &node) {
            context.diagnostics.add_error(node.span,
                                          "captured closures may only be invoked directly");
        }
    }

    void visit(BlockExpression& node) override {
        visit(*node.body);
        if (!node.body->statements.empty()) {
            if (auto* tail = node.body->statements.back()->as<ExpressionStatement>(); tail != nullptr) {
                tail->expression = sema.binding.construct(tail->expression, tail->expression->type, resolver, context.diagnostics);
            }
        }
        node.type = node.body->type;
    }

    void visit(ArrayLiteralExpression& node) override {
        const auto* array_type =
            target_type != nullptr ? resolver.resolve_type(target_type)->as<ArrayType>() : nullptr;
        const auto inferred = target_type == nullptr;

        if (inferred && !node.elements.empty()) {
            visit_expression(*node.elements.front());
            const auto* element_type = resolver.resolve_type(node.elements.front()->type);

            if (element_type == nullptr || element_type->is<VoidType>() || element_type->is<NeverType>()) {
                context.diagnostics.add_error(node.span, "array elements must have a concrete value type");

                return;
            }

            array_type = sema.types.create<ArrayType>(element_type,
                ConcreteArrayExtent(node.elements.size()));
        }

        if (array_type == nullptr) {
            context.diagnostics.add_error(node.span, inferred
                ? "empty array literal requires an array target type"
                : "array literal requires an array target type");
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

        for (auto index = std::size_t{0}; index < node.elements.size(); ++index) {
            auto*& element = node.elements[index];
            StateGuard target_guard(target_type, array_type->element);

            if (!inferred || index != 0) {
                visit_expression(*element);
            }

            if (element->type == nullptr || !compatibility.accepts(array_type->element, element->type)) {
                context.diagnostics.add_error(element->span, "array element does not match its element type");

                return;
            }

            element = sema.binding.convert(element, array_type->element, resolver);
            element = sema.binding.construct(element, array_type->element, resolver, context.diagnostics);
        }

        node.type = array_type;
    }

    explicit SemanticChecker(Context& context, SemaContext& sema,
                             const CompileTimeEnvironment* environment = nullptr)
        : context(context), sema(sema), resolver(sema.types, sema.env, context.diagnostics),
          builder(context, sema, resolver, *this),
          facades(sema.facades, resolver), compatibility(sema.types, resolver, facades),
          declarations(context, sema, resolver, builder, *this,
                       [this](Expression& expression, const Type* target) {
                           visit_expression_with_target(expression, target);
                       }) {
        if (environment != nullptr) {
            resolver.use_environment(*environment);
        }
    }

    void check(Program& program) {
        declarations.declare_program_symbols(program);

        std::unordered_set<std::string> tests;
        tests.reserve(program.statements.size());

        for (auto* statement : program.statements) {
            if (const auto* test = statement->as<TestDeclaration>(); test != nullptr &&
                !tests.insert(test->name).second) {
                context.diagnostics.add_error(test->name_span, "duplicate test name '" + test->name + "'");
            }
        }

        for (auto* statement : program.statements) {
            if (statement->as<VarDeclaration>() != nullptr) {
                visit_statement(*statement);
            }
        }

        for (auto* statement : program.statements) {
            if (statement->as<VarDeclaration>() == nullptr) {
                visit_statement(*statement);
            }
        }
    }
};

