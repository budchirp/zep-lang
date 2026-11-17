module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.lowerer;

import zep.common.source.span;
import zep.common.context;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.sema.scopes.environment;
import zep.frontend.kind;
import zep.frontend.sema.declaration.attribute;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.lowerer.lifetime;
import zep.lowerer.mangler;
import zep.lowerer.specialization;
import zep.lowerer.nodes;
import zep.common.state;
import zep.frontend.sema.lifecycle;

export class Lowerer : public Visitor<HIRNode*> {
  private:
    SemaContext& sema;
    std::shared_ptr<HIRProgram> program;

    TypeResolver resolver;
    Facades facades;

    TemporaryNames names;
    Specializations specializations;
    Construction construction;
    Cleanup cleanup;
    Temporaries temporaries;

    std::unordered_map<const Type*, const Type*> lower_type_cache;
    std::unordered_map<std::string, const Type*> runtime_types;
    std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>>
        captured_variables;
    bool consuming_block_tail = false;

    HIRCallExpression* make_call(CallExpression& node, std::unique_ptr<HIRCallTarget> target,
                                 std::vector<HIRExpression*> arguments,
                                 std::vector<HIRStatement*> transfers = {}) {
        const auto* result = node.reference_result != nullptr ? node.reference_result : node.type;
        const auto* direct = target->as<HIRDirectCallTarget>();
        auto* call = program->nodes.create<HIRCallExpression>(
            node.span, std::move(target), std::move(arguments), lower_type(result, node.span));

        if (direct == nullptr || direct->abi == Abi::Type::Language) {
            call->transfers = std::move(transfers);
        }

        if (const auto decisions = sema.errors.targets.find(&node); decisions != sema.errors.targets.end()) {
            call->failures.reserve(decisions->second.size());

            for (const auto& failure : decisions->second) {
                auto edge = temporaries.failure(
                    lower_type(failure.type, node.span), failure.handler, node.span, diagnostics);

                if (!edge.has_value()) {
                    return nullptr;
                }

                call->failures.push_back(std::move(*edge));
            }
        }

        return call;
    }

    HIRCallExpression* bind_call(CallExpression& node, HIRExpression* callee,
                                 std::vector<HIRExpression*> arguments,
                                 std::vector<HIRStatement*> transfers = {}) {
        if (node.resolved_target == nullptr) {
            diagnostics.add_error(node.span, "call has no resolved semantic target");

            return nullptr;
        }

        if (const auto* direct = node.resolved_target->as<DirectCallTarget>(); direct != nullptr) {
            const auto* identifier = callee != nullptr ? callee->as<HIRIdentifierExpression>() : nullptr;

            if (identifier == nullptr || identifier->name.empty()) {
                diagnostics.add_error(node.span, "direct call has no lowered callable");

                return nullptr;
            }

            const auto* function = identifier->function_symbol != nullptr
                ? identifier->function_symbol : direct->function_symbol;
            const auto* signature = identifier->type != nullptr
                ? identifier->type->as<FunctionType>() : lower_function_type(direct->signature, node.span);

            return make_call(node, std::make_unique<HIRDirectCallTarget>(*function,
                identifier->name, signature), std::move(arguments), std::move(transfers));
        }

        if (const auto* indirect = node.resolved_target->as<IndirectCallTarget>(); indirect != nullptr) {
            if (callee == nullptr) {
                return nullptr;
            }

            const auto* signature = lower_function_type(indirect->function_type, node.span);

            return make_call(node, std::make_unique<HIRIndirectCallTarget>(callee, *signature),
                              std::move(arguments), std::move(transfers));
        }

        if (const auto* dispatch = node.resolved_target->as<InterfaceCallTarget>(); dispatch != nullptr) {
            return make_call(node, std::make_unique<HIRInterfaceCallTarget>(
                *dispatch->method_symbol, dispatch->slot, lower_function_type(dispatch->signature, node.span)),
                std::move(arguments), std::move(transfers));
        }

        diagnostics.add_error(node.span, "call has no resolved semantic target");

        return nullptr;
    }

    HIRExpression* specialize_call(CallExpression& node, const std::string& name,
                                   const FunctionType* signature,
                                   const FunctionDeclaration* definition,
                                   const std::vector<GenericBinding>& bindings) {
        for (const auto& binding : bindings) {
            if (!is_concrete_binding(binding)) {
                diagnostics.add_error(node.span, "generic specialization requires concrete bindings");

                return nullptr;
            }
        }

        const auto identity = definition != nullptr ? std::variant<const Node*, const Type*>(definition)
            : std::variant<const Node*, const Type*>(signature);
        const auto instance = specializations.get_or_create(identity, name, bindings);

        if (!instance.has_value()) {
            diagnostics.add_error(node.span, "generic specialization requires concrete bindings");

            return nullptr;
        }

        if (!instance->is_generated && definition != nullptr) {
            auto* body = specialize(*definition, instance->name, bindings);

            if (body != nullptr) {
                specializations.enqueue(body);
            }
        }

        const Type* type = nullptr;

        if (definition != nullptr) {
            auto scope = resolver.create_substitution_scope();
            const auto* declared = definition->type != nullptr ? definition->type->as<FunctionType>() : nullptr;

            if (declared != nullptr) {
                const auto count = std::min(declared->generic_parameters.size(), bindings.size());

                for (auto index = std::size_t{0}; index < count; ++index) {
                    bind_parameter(declared->generic_parameters[index], bindings[index]);
                }
            }

            type = lower_type(definition->type, node.span);
        } else {
            type = lower_type(signature, node.span);
        }

        return program->nodes.create<HIRIdentifierExpression>(node.callee->span, instance->name, type);
    }

    HIRCallExpression* lower_captured_call(CallExpression& node, ClosureExpression& closure) {
        const auto* signature = lower_function_type(closure.type, node.span);

        if (signature == nullptr || signature->parameters.size() != node.arguments.size()) {
            return nullptr;
        }

        auto name = names.next("closure");
        std::vector<ParameterType> parameters;
        parameters.reserve(closure.captures.size() + signature->parameters.size());

        std::vector<HIRExpression*> arguments;
        arguments.reserve(closure.captures.size() + node.arguments.size());
        std::vector<HIRStatement*> transfers;
        transfers.reserve(node.arguments.size());

        std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>> captures;
        captures.reserve(closure.captures.size());

        for (auto index = std::size_t{0}; index < closure.captures.size(); ++index) {
            const auto* symbol = closure.captures[index];

            if (symbol == nullptr || symbol->type == nullptr) {
                return nullptr;
            }

            const auto* type = lower_type(symbol->type, node.span);
            const auto* pointer = sema.types.create<PointerType>(type, true);
            auto parameter = "__capture_" + std::to_string(index);
            parameters.emplace_back(parameter, pointer);
            captures.emplace(symbol, std::make_pair(parameter, pointer));

            auto* value = program->nodes.create<HIRIdentifierExpression>(
                node.span, Mangler::linker_name(symbol->name, symbol), type, symbol);
            arguments.push_back(program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::AddressOfMut, value, pointer));
        }

        for (const auto& parameter : signature->parameters) {
            parameters.emplace_back(parameter.name, lower_type(parameter.type, node.span));
        }

        const auto* function_type = sema.types.create<FunctionType>(name,
            lower_type(signature->return_type, node.span), std::move(parameters),
            std::vector<GenericParameterType>(), false, signature->errors);
        HIRBlockStatement* body = nullptr;

        {
            if (closure.body->scope == nullptr) {
                diagnostics.add_error(closure.span, "closure has no checked scope");

                return nullptr;
            }

            ScopeGuard scope(sema.env.current_scope, closure.body->scope->parent);
            StateGuard capture_guard(captured_variables, std::move(captures));
            body = build_owned_body(closure.parameters, function_type, *closure.body, closure.captures.size());
        }

        if (body == nullptr) {
            return nullptr;
        }

        program->statements.push_back(program->nodes.create<HIRFunctionDeclaration>(node.span,
            Visibility::Type::Private, Linkage::Type::Internal, name, function_type->return_type,
            body, false, function_type, false, Abi::Type::Language));

        for (const auto* argument : node.arguments) {
            auto* value = lower_expression(*argument->value);

            if (value == nullptr) {
                return nullptr;
            }

            arguments.push_back(temporaries.capture(value, argument->span, &transfers));
        }

        return make_call(node, std::make_unique<HIRDirectCallTarget>(name, function_type),
                          std::move(arguments), std::move(transfers));
    }

    HIRCallExpression* lower_call(CallExpression& node,
        std::optional<std::vector<HIRExpression*>> bound = std::nullopt) {
        if (auto* closure = node.callee->as<ClosureExpression>();
            closure != nullptr && !closure->captures.empty()) {
            return lower_captured_call(node, *closure);
        }

        if (node.resolved_target == nullptr) {
            diagnostics.add_error(node.span, "call has no resolved semantic target");

            return nullptr;
        }

        std::vector<HIRExpression*> arguments;
        std::vector<HIRStatement*> transfers;
        transfers.reserve(node.arguments.size());

        if (bound.has_value()) {
            arguments = std::move(*bound);
        } else {
            arguments.reserve(node.arguments.size());

            for (const auto* argument : node.arguments) {
                arguments.push_back(temporaries.capture(lower_expression(*argument->value), argument->span, &transfers));
            }
        }

        const auto* target = node.resolved_target->as<DirectCallTarget>();

        if (node.resolved_target->as<InterfaceCallTarget>() != nullptr) {
            return bind_call(node, nullptr, std::move(arguments), std::move(transfers));
        }

        const auto* function = target != nullptr ? target->function_symbol : nullptr;

        if (function == nullptr) {
            const auto* signature = node.callee->type != nullptr ? node.callee->type->as<FunctionType>() : nullptr;
            auto* callee = signature == nullptr || node.generic_arguments.empty()
                ? lower_expression(*node.callee)
                : specialize_call(node, signature->name, signature,
                    specializations.get_function(signature->name),
                    lower_generic_arguments(node.generic_arguments, node.span));

            return bind_call(node, callee, std::move(arguments), std::move(transfers));
        }

        const auto* signature = function->signature();

        if (signature == nullptr) {
            return nullptr;
        }

        std::vector<GenericBinding> bindings;
        bindings.reserve(node.bindings.size());

        for (const auto& binding : node.bindings) {
            const auto resolved = resolver.resolve_binding(binding);

            if (const auto* type = std::get_if<TypeBinding>(&resolved); type != nullptr) {
                bindings.emplace_back(TypeBinding(lower_type(type->type, node.span)));
            } else {
                bindings.push_back(resolved);
            }
        }

        const auto name = Mangler::function_name(function->base_name(), function, signature);

        if (!bindings.empty()) {
            auto* callee = specialize_call(
                node, name, signature, specializations.get_function(function), bindings);

            return bind_call(node, callee, std::move(arguments), std::move(transfers));
        }

        const auto* direct = node.resolved_target->as<DirectCallTarget>();
        const auto* resolved = lower_function_type(direct != nullptr ? direct->signature : signature, node.span);
        auto emitted = Mangler::function_name(function->base_name(), function, resolved);

        auto* callee = program->nodes.create<HIRIdentifierExpression>(node.callee->span, emitted, resolved);

        return bind_call(node, callee, std::move(arguments), std::move(transfers));
    }

    HIRExpression* lower_expression(Expression& node) {
        if (diagnostics.has_errors()) {
            return nullptr;
        }

        return static_cast<HIRExpression*>(visit_expression(node));
    }

    bool check_scope(Scope* scope, Span span) const {
        if (scope == nullptr) {
            diagnostics.add_error(span, "node has no checked semantic scope");

            return false;
        }

        return true;
    }

    HIRStatement* lower_statement(Statement& node) {
        if (diagnostics.has_errors()) {
            return nullptr;
        }

        FullExpression expression(temporaries.current, cleanup.scopes.size());
        StateGuard guard(temporaries.current, &expression);
        auto* statement = static_cast<HIRStatement*>(visit_statement(node));

        if (diagnostics.has_errors()) {
            return nullptr;
        }

        if (expression.declarations.empty()) {
            return statement;
        }

        auto* expression_statement = statement != nullptr ? statement->as<HIRExpressionStatement>() : nullptr;

        if (expression_statement != nullptr && expression_statement->expression != nullptr &&
            expression_statement->expression->type != nullptr &&
            !expression_statement->expression->type->is<VoidType>() &&
            !expression_statement->expression->type->is<NeverType>()) {
            auto* result = temporaries.complete(node.span, expression_statement->expression, expression);

            return program->nodes.create<HIRExpressionStatement>(node.span, result);
        }

        std::vector<HIRStatement*> statements;
        statements.reserve(expression.declarations.size() + expression.drops.size() + 1);
        statements.insert(statements.end(), expression.declarations.begin(), expression.declarations.end());

        if (statement != nullptr) {
            statements.push_back(statement);
        }

        if (node.as<ReturnStatement>() == nullptr && !terminates(statement)) {
            statements.insert(statements.end(), expression.drops.begin(), expression.drops.end());
        }

        return program->nodes.create<HIRStatementGroup>(node.span, std::move(statements));
    }

    HIRExpression* lower_condition(Expression& node) {
        FullExpression expression(temporaries.current, cleanup.scopes.size());
        StateGuard guard(temporaries.current, &expression);
        auto* value = lower_expression(node);

        return temporaries.complete(node.span, value, expression);
    }

    HIRBlockStatement* lower_block(Statement& node) {
        return static_cast<HIRBlockStatement*>(lower_statement(node));
    }

    const Type* lower_type(const Type* type, Span span) {
        if (type == nullptr) {
            return nullptr;
        }

        const auto* nominal = type->as_nominal();
        const auto* resolved = nominal != nullptr && resolver.is_concrete(nominal)
            ? type : resolver.resolve_type(type);

        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto cached = lower_type_cache.find(resolved); cached != lower_type_cache.end()) {
            return cached->second;
        }

        const Type* lowered = resolved;

        if (const auto* backing = facades.backing(resolved); backing != nullptr) {
            lowered = lower_type(backing, span);
        } else if (const auto* pointer = resolved->as<PointerType>(); pointer != nullptr) {
            lowered = sema.types.create<PointerType>(
                lower_type(pointer->element, span), pointer->is_mutable, pointer->reference_kind);
        } else if (const auto* array = resolved->as<ArrayType>(); array != nullptr) {
            lowered = sema.types.create<ArrayType>(lower_type(array->element, span), array->extent);
        } else if (const auto* function = resolved->as<FunctionType>(); function != nullptr) {
            std::vector<ParameterType> parameters;
            parameters.reserve(function->parameters.size());
            std::vector<const Type*> failures;
            failures.reserve(function->errors.size());

            for (const auto* error : function->errors) {
                failures.push_back(lower_type(error, span));
            }

            for (const auto& parameter : function->parameters) {
                parameters.emplace_back(parameter.name, lower_type(parameter.type, span));
            }

            lowered = sema.types.create<FunctionType>(function->name,
                lower_type(function->return_type, span), std::move(parameters),
                std::vector<GenericParameterType>(), function->variadic, std::move(failures));
        } else if (const auto* structure = resolved->as<StructType>(); structure != nullptr) {
            lowered = lower_struct_type(type, structure, span);
        } else if (const auto* enumeration = resolved->as<EnumType>(); enumeration != nullptr) {
            lowered = lower_enum_type(enumeration, span);
        } else if (const auto* interface = resolved->as<InterfaceType>(); interface != nullptr) {
            lowered = lower_interface_type(interface, span);
        }

        lower_type_cache.emplace(resolved, lowered);

        return lowered;
    }

    const FunctionType* lower_function_type(const Type* type, Span span) {
        const auto* lowered = lower_type(type, span);

        return lowered != nullptr ? lowered->as<FunctionType>() : nullptr;
    }

    const Type* prepare_lifecycle(const Type* type, Span span, Operation::Type operation) {
        const auto* lowered = lower_type(type, span);
        const auto* structure = lowered != nullptr ? lowered->as<StructType>() : nullptr;
        if (structure == nullptr || structure->generic_arguments.empty()) {
            return lowered;
        }

        auto scope = resolver.create_substitution_scope();
        const auto arguments = lower_generic_arguments(structure->generic_arguments, span);
        const auto& parameters = structure->definition->generic_parameters;
        const auto count = std::min(parameters.size(), arguments.size());
        for (auto index = std::size_t{0}; index < count; ++index) {
            bind_parameter(parameters[index], arguments[index]);
        }

        resolver.bind_type_parameter(structure->name, structure);
        resolver.bind_type_parameter("Self", structure);
        const auto plan = sema.lifecycle.resolve(structure, operation, resolver);
        const auto* function = plan->function;
        const auto* definition = specializations.get_function(function);

        if (function == nullptr || sema.lifecycle.deleted(function) || definition == nullptr) {
            return lowered;
        }

        resolver.bind_type_parameter(function->parent, structure);
        const auto* signature = lower_function_type(function->signature(), span);

        if (signature == nullptr) {
            return lowered;
        }

        const auto instance = specializations.lifecycle(*function, *structure, signature);

        if (!instance.has_value()) {
            diagnostics.add_error(span, "lifecycle specialization requires concrete bindings");

            return nullptr;
        }

        if (!instance->is_generated) {
            StateGuard function_guard(cleanup.scopes, std::vector<CleanupScope>());

            if (auto* specialization = specialize(*definition, instance->name, arguments);
                specialization != nullptr) {
                specializations.enqueue(specialization);
            }
        }

        return lowered;
    }

    static std::size_t parameter_offset(const FunctionType* function_type,
                                        std::size_t declared_count) {
        if (function_type == nullptr || function_type->parameters.size() <= declared_count) {
            return 0;
        }

        return function_type->parameters.size() - declared_count;
    }

    bool terminates(HIRStatement* statement) const {
        if (statement == nullptr) {
            return false;
        }

        if (statement->as<HIRReturnStatement>() != nullptr) {
            return true;
        }

        if (auto* group = statement->as<HIRStatementGroup>(); group != nullptr) {
            if (group->statements.empty()) {
                return false;
            }

            return terminates(group->statements.back());
        }

        auto* block = statement->as<HIRBlockStatement>();
        if (block == nullptr || block->statements.empty()) {
            return false;
        }

        return terminates(block->statements.back());
    }

    bool is_emittable_type(const Type* type, std::unordered_set<const Type*>& visited) {
        if (type == nullptr || !visited.insert(type).second) {
            return true;
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            for (const auto& argument : named_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }

            if (named_type->name == "Self") {
                return true;
            }
        }

        const auto* resolved = resolver.resolve_type(type);
        if (resolved != nullptr && resolved != type) {
            return is_emittable_type(resolved, visited);
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            return false;
        }

        if (const auto* pointer_type = type->as<PointerType>(); pointer_type != nullptr) {
            return is_emittable_type(pointer_type->element, visited);
        }

        if (const auto* array_type = type->as<ArrayType>(); array_type != nullptr) {
            return !std::holds_alternative<DependentArrayExtent>(array_type->extent) &&
                   is_emittable_type(array_type->element, visited);
        }

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            return function_type->generic_parameters.empty();
        }

        if (const auto* nominal_type = type->as_nominal(); nominal_type != nullptr) {
            if (!nominal_type->generic_parameters.empty()) {
                return false;
            }

            for (const auto& argument : nominal_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }
        }

        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                if (!is_emittable_type(field.type, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            if (!is_emittable_type(enum_type->backing_type, visited)) {
                return false;
            }

            for (const auto& variant : enum_type->variants) {
                for (const auto& field : variant.fields) {
                    if (!is_emittable_type(field.type, visited)) {
                        return false;
                    }
                }
            }

            return true;
        }

        if (const auto* interface_type = type->as<InterfaceType>(); interface_type != nullptr) {
            for (const auto* parent_interface : interface_type->interfaces) {
                if (!is_emittable_type(parent_interface, visited)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool is_emittable_function_type(const FunctionType* function_type) {
        if (function_type == nullptr || !function_type->generic_parameters.empty()) {
            return false;
        }

        std::unordered_set<const Type*> visited;
        if (!is_emittable_type(function_type->return_type, visited)) {
            return false;
        }

        for (const auto& parameter : function_type->parameters) {
            if (!is_emittable_type(parameter.type, visited)) {
                return false;
            }
        }

        return true;
    }

    void bind_parameter(const GenericParameterType& parameter, const GenericBinding& binding) {
        resolver.bind_generic_binding(parameter.name, binding, parameter.declaration);
    }

    bool is_concrete_binding(const GenericBinding& binding) const {
        const auto* type = std::get_if<TypeBinding>(&binding);
        if (type != nullptr) {
            return resolver.is_concrete(type->type);
        }

        return std::get<ConstBinding>(binding).value.has_value();
    }

    void apply_substitutions(const std::vector<GenericParameter*>& parameters,
                             const std::vector<GenericBinding>& arguments) {
        const auto count = std::min(parameters.size(), arguments.size());

        for (std::size_t index = 0; index < count; ++index) {
            resolver.bind_generic_binding(parameters[index]->name, arguments[index],
                                          parameters[index]);
        }
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgumentType>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto& argument : arguments) {
            if (argument.is_const()) {
                bindings.push_back(resolver.resolve_binding(argument.binding()));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument.type, span)));
            }
        }

        return bindings;
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgument*>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto* argument : arguments) {
            if (argument->const_binding.has_value()) {
                bindings.push_back(resolver.resolve_binding(*argument->const_binding));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument->type->type, span)));
            }
        }

        return bindings;
    }

    const Type* lower_enum_type(const EnumType* type, Span span) {
        if (!type->generic_parameters.empty() || !resolver.is_concrete(type)) {
            return type;
        }

        std::vector<EnumVariantType> variants;
        variants.reserve(type->variants.size());

        const auto* backing = lower_type(type->backing_type, span);
        auto changed = backing != type->backing_type;

        std::vector<FieldType> shared;
        shared.reserve(type->fields.size());

        for (const auto& field : type->fields) {
            const auto* lowered = lower_type(field.type, span);
            changed = changed || lowered != field.type;
            shared.emplace_back(field.name, lowered, field.visibility);
        }

        for (const auto& variant : type->variants) {
            std::vector<FieldType> fields;
            fields.reserve(variant.fields.size());

            for (const auto& field : variant.fields) {
                const auto* lowered = lower_type(field.type, span);
                changed = changed || lowered != field.type;
                fields.emplace_back(field.name, lowered);
            }

            variants.emplace_back(variant.name, variant.index, std::move(fields), variant.discriminant);
        }

        const auto* lowered = changed
            ? sema.types.create<EnumType>(type->name, std::vector<GenericParameterType>(),
                std::move(variants), backing, type->generic_arguments, type->interfaces,
                type->methods, type->definition, std::move(shared))
            : type;

        reference_type(type_name(*type), lowered);

        return lowered;
    }

    const Type* lower_interface_type(const InterfaceType* type, Span span) {
        if (!type->generic_parameters.empty() || !resolver.is_concrete(type)) {
            return type;
        }

        std::vector<MethodType> methods;
        methods.reserve(type->methods.size());

        for (const auto& method : type->methods) {
            methods.emplace_back(method.name, lower_function_type(method.type, span), method.index);
        }

        auto* lowered = sema.types.create<InterfaceType>(type->name,
            std::vector<GenericParameterType>(), std::move(methods), type->interfaces,
            type->generic_arguments, type->definition);

        reference_type(type_name(*type), lowered);

        return lowered;
    }

    const Type* lower_struct_type(const Type* source, const StructType* type, Span span) {
        if (type->generic_parameters.empty() && !type->generic_arguments.empty() &&
            type->definition != type && type->name != type->definition->name) {
            return type;
        }

        const auto* definition = specializations.get_struct(type);
        std::vector<GenericBinding> bindings;

        if (const auto* named = source->as<NamedType>();
            named != nullptr && !named->generic_arguments.empty()) {
            bindings = lower_generic_arguments(named->generic_arguments, span);
        } else if (!type->generic_arguments.empty()) {
            bindings = lower_generic_arguments(type->generic_arguments, span);
        } else if (definition != nullptr && !definition->generic_parameters.empty()) {
            bindings.reserve(definition->generic_parameters.size());

            for (const auto* parameter : definition->generic_parameters) {
                const auto constant = resolver.const_parameter(parameter->name, parameter);

                if (parameter->is_const() && constant.has_value()) {
                    bindings.emplace_back(*constant);
                } else {
                    const auto* named = sema.types.create<NamedType>(
                        parameter->name, std::vector<GenericArgumentType>(), parameter);
                    bindings.emplace_back(TypeBinding(resolver.resolve_type(named)));
                }
            }
        }

        if (bindings.empty()) {
            reference_type(type_name(*type), type);

            return type;
        }

        for (const auto& binding : bindings) {
            if (!is_concrete_binding(binding)) {
                return type;
            }
        }

        const auto identity = definition != nullptr ? std::variant<const Node*, const Type*>(definition)
            : std::variant<const Node*, const Type*>(type);
        const auto instance = specializations.get_or_create(identity, Scope::qualified_name(*type), bindings);

        if (!instance.has_value()) {
            diagnostics.add_error(span, "generic specialization requires concrete bindings");

            return nullptr;
        }

        if (const auto existing = runtime_types.find(instance->name); existing != runtime_types.end()) {
            return existing->second;
        }

        std::vector<GenericArgumentType> arguments;
        arguments.reserve(bindings.size());

        for (auto index = std::size_t{0}; index < bindings.size(); ++index) {
            const auto name = definition != nullptr && index < definition->generic_parameters.size()
                ? definition->generic_parameters[index]->name : std::string();

            arguments.emplace_back(name, bindings[index]);
        }

        auto* structure = sema.types.create<StructType>(instance->name,
            std::vector<GenericParameterType>(), std::vector<FieldType>(), std::move(arguments),
            nullptr, std::vector<const InterfaceType*>(), std::vector<MethodType>(), type->definition);

        reference_type(instance->name, structure);
        auto scope = resolver.create_substitution_scope();

        if (definition != nullptr) {
            apply_substitutions(definition->generic_parameters, bindings);
        }

        structure->base_type = type->base_type != nullptr
            ? lower_type(type->base_type, span)->as<StructType>() : nullptr;
        structure->interfaces.reserve(type->interfaces.size());

        for (const auto* interface : type->interfaces) {
            const auto* lowered = lower_type(interface, span);
            structure->interfaces.push_back(lowered != nullptr ? lowered->as<InterfaceType>() : nullptr);
        }

        structure->fields.reserve(type->fields.size());

        for (const auto& field : type->fields) {
            structure->fields.emplace_back(field.name, lower_type(field.type, span), field.visibility);
        }

        structure->methods = type->methods;

        if (definition == nullptr) {
            return structure;
        }

        for (const auto* method : definition->methods) {
            auto required = method->is_override;

            for (const auto* interface : structure->interfaces) {
                if (interface == nullptr) {
                    continue;
                }

                for (const auto& entry : interface->methods) {
                    required = required || entry.name == method->prototype->name;
                }
            }

            if (!required || method->is_deleted) {
                continue;
            }

            const auto* signature = lower_function_type(method->type, method->span);

            if (signature == nullptr) {
                continue;
            }

            const auto name = Mangler::function_name(method->function_symbol != nullptr
                ? method->function_symbol->base_name() : method->prototype->name,
                method->function_symbol, signature);

            const auto instance = specializations.get_or_create(method, name, bindings);

            if (!instance.has_value()) {
                diagnostics.add_error(method->span, "generic specialization requires concrete bindings");

                return nullptr;
            }

            if (instance->is_generated) {
                continue;
            }

            auto* specialization = specialize(*method, instance->name, bindings);

            if (specialization != nullptr) {
                specializations.enqueue(specialization);
            }
        }

        return structure;
    }

    std::string type_name(const NominalType& type) const {
        return Mangler::encode(&type);
    }

    HIRFunctionDeclaration* specialize(const FunctionDeclaration& declaration,
                                       const std::string& name,
                                       const std::vector<GenericBinding>& arguments) {
        if (!check_scope(declaration.scope, declaration.span)) {
            return nullptr;
        }

        ScopeGuard function_scope(sema.env.current_scope, declaration.scope);
        auto scope = resolver.create_substitution_scope();
        const auto* signature = declaration.type != nullptr ? declaration.type->as<FunctionType>() : nullptr;

        if (signature == nullptr) {
            diagnostics.add_error(declaration.span, "function has no checked signature");

            return nullptr;
        }

        const auto count = std::min(signature->generic_parameters.size(), arguments.size());

        for (auto index = std::size_t{0}; index < count; ++index) {
            bind_parameter(signature->generic_parameters[index], arguments[index]);
        }

        const auto* type = lower_function_type(declaration.type, declaration.span);

        if (type == nullptr) {
            diagnostics.add_error(declaration.span, "specialization has no lowered signature");

            return nullptr;
        }

        if (!is_emittable_function_type(type)) {
            diagnostics.add_error(declaration.span,
                "specialization has an unresolved signature: " + type->to_string());

            return nullptr;
        }

        auto* body = build_body(declaration, type);

        if (body == nullptr) {
            return nullptr;
        }

        return program->nodes.create<HIRFunctionDeclaration>(declaration.span, declaration.visibility,
            Linkage::Type::LinkOnceODR, name, type->return_type, body, declaration.prototype->is_variadic,
            type, Mangler::is_mangled(declaration.function_symbol), Abi::Type::Language);
    }

    HIRExpression* lower_compile_time_value(Span span, CompileTimeValue value) {
        const auto* type = lower_type(value.type, span);

        switch (value.kind) {
        case CompileTimeValue::Kind::Type::SignedInteger:
            return program->nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::int64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::UnsignedInteger:
            return program->nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::uint64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::Float:
            return program->nodes.create<HIRFloatLiteral>(span, value.to_string(), type);

        case CompileTimeValue::Kind::Type::Boolean:
            return program->nodes.create<HIRBooleanLiteral>(
                span, std::get<bool>(value.payload), type);

        case CompileTimeValue::Kind::Type::Char:
            return program->nodes.create<HIRCharLiteral>(
                span, std::get<std::uint8_t>(value.payload), type);

        case CompileTimeValue::Kind::Type::String:
            return program->nodes.create<HIRStringLiteral>(
                span, std::get<std::string>(value.payload), type);

        case CompileTimeValue::Kind::Type::Null:
            return program->nodes.create<HIRNullLiteral>(span, type);

        case CompileTimeValue::Kind::Type::Struct: {
            const auto* structure = type != nullptr ? type->as<StructType>() : nullptr;
            const auto& values = std::get<std::vector<CompileTimeValue>>(value.payload);
            if (structure == nullptr || structure->fields.size() != values.size()) {
                diagnostics.add_error(span, "constant struct requires concrete field types");

                return nullptr;
            }

            std::vector<HIRStructLiteralField> fields;
            fields.reserve(values.size());
            for (auto index = std::size_t{0}; index < values.size(); ++index) {
                auto* expression = lower_compile_time_value(span, values[index]);
                if (expression == nullptr) {
                    diagnostics.add_error(
                        span, "constant struct field cannot be represented at runtime");

                    return nullptr;
                }
                fields.emplace_back(structure->fields[index].name, expression);
            }
            return program->nodes.create<HIRStructLiteralExpression>(
                span, structure->name, std::move(fields), type);
        }
        case CompileTimeValue::Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(value.payload);
            const auto* array = type != nullptr ? type->as<ArrayType>() : nullptr;
            if (array == nullptr || array->element == nullptr) {
                diagnostics.add_error(span, "constant array requires a concrete element type");

                return nullptr;
            }
            if (!array->element->is<StringType>()) {
                std::vector<HIRExpression*> expressions;
                expressions.reserve(elements.size());
                for (const auto& element : elements) {
                    auto* expression = lower_compile_time_value(span, element);
                    if (expression == nullptr) {
                        diagnostics.add_error(
                            span, "constant array element cannot be represented at runtime");

                        return nullptr;
                    }
                    expressions.push_back(expression);
                }
                return program->nodes.create<HIRArrayLiteralExpression>(
                    span, std::move(expressions), type);
            }
            std::vector<std::string> strings;
            strings.reserve(elements.size());
            for (const auto& element : elements) {
                if (element.kind != CompileTimeValue::Kind::Type::String) {
                    diagnostics.add_error(
                        span, "constant array element cannot be represented at runtime");

                    return nullptr;
                }
                strings.push_back(std::get<std::string>(element.payload));
            }
            return program->nodes.create<HIRStringArrayExpression>(span, std::move(strings),
                                                                           type);
        }
        }

        return nullptr;
    }

    std::vector<HIRStructLiteralField>
    lower_fields(std::vector<StructLiteralField*>& fields, std::vector<HIRStatement*>& transfers) {
        std::vector<HIRStructLiteralField> result;
        result.reserve(fields.size());

        for (auto* field : fields) {
            result.emplace_back(field->name,
                temporaries.capture(lower_expression(*field->value), field->span, &transfers));
        }

        return result;
    }

    HIRBlockStatement* build_defaulted_constructor(const FunctionDeclaration& declaration, const FunctionType* type) {
        const auto* symbol = declaration.function_symbol;
        if (symbol == nullptr || !symbol->has_attribute(".defaulted_constructor")) {
            return nullptr;
        }
        const auto& parameter = type->parameters.front();
        const auto* pointer_type = parameter.type->as<PointerType>();
        auto* pointer = program->nodes.create<HIRIdentifierExpression>(declaration.span, parameter.name, parameter.type);
        auto* source = program->nodes.create<HIRUnaryExpression>(declaration.span, UnaryOperator::Type::Dereference,
            pointer, pointer_type->element);
        const auto operation = symbol->has_role(ConstructorKind::Type::Move)
            ? Operation::Type::Move : Operation::Type::Copy;
        auto* result = construction.construct(declaration.span, source,
            sema.lifecycle.resolve(type->return_type, operation, resolver, LifecycleMode::Type::Defaulted));
        std::vector<HIRStatement*> statements;
        statements.reserve(1);
        statements.push_back(program->nodes.create<HIRReturnStatement>(declaration.span, result));
        return program->nodes.create<HIRBlockStatement>(declaration.span, std::move(statements));
    }

    HIRBlockStatement* build_owned_body(const std::vector<Parameter*>& source_parameters,
                                        const FunctionType* function_type, BlockStatement& body,
                                        std::size_t hidden_parameter_count = 0) {
        StateGuard function_guard(cleanup.scopes, std::vector<CleanupScope>());
        StateGuard error_guard(temporaries.handlers, decltype(temporaries.handlers)());
        StateGuard expression_guard(temporaries.current, static_cast<FullExpression*>(nullptr));
        StateGuard tail_guard(consuming_block_tail, false);
        CleanupScopeGuard scope_guard(cleanup);

        std::vector<HIRStatement*> parameter_flags;
        parameter_flags.reserve(source_parameters.size());

        auto offset = parameter_offset(function_type, source_parameters.size());
        if (offset < hidden_parameter_count) {
            return nullptr;
        }

        for (auto index = std::size_t{0}; index < source_parameters.size(); ++index) {
            auto* parameter = source_parameters[index];
            const auto parameter_index = index + offset;
            const auto* lowered_type =
                function_type != nullptr && parameter_index < function_type->parameters.size()
                    ? function_type->parameters[parameter_index].type
                    : nullptr;

            if (auto* flag = cleanup.register_value(parameter->name, parameter->name,
                                                     parameter->span, lowered_type);
                flag != nullptr) {
                parameter_flags.push_back(flag);
            }
        }

        auto* lowered_body = static_cast<HIRBlockStatement*>(lower_statement(body));

        if (lowered_body == nullptr) {
            return nullptr;
        }
        lowered_body->statements.insert(lowered_body->statements.begin(), parameter_flags.begin(),
                                        parameter_flags.end());

        if (!terminates(lowered_body)) {
            auto parameter_destruction = cleanup.emit();
            lowered_body->statements.insert(lowered_body->statements.end(),
                                            parameter_destruction.begin(),
                                            parameter_destruction.end());
        }

        return lowered_body;
    }

    HIRBlockStatement* build_body(const FunctionDeclaration& declaration, const FunctionType* type) {
        auto* body = build_defaulted_constructor(declaration, type);

        if (diagnostics.has_errors()) {
            return nullptr;
        }

        if (body != nullptr) {
            return body;
        }

        if (declaration.body == nullptr) {
            diagnostics.add_error(declaration.span, "function has no checked body");

            return nullptr;
        }

        return build_owned_body(declaration.prototype->parameters, type, *declaration.body);
    }

    HIRNode* lower_identifier(IdentifierExpression& node) {
        const auto* var_symbol = node.var_symbol;
        auto name = Mangler::linker_name(node.name, var_symbol);
        const auto* source_type =
            var_symbol != nullptr && var_symbol->type != nullptr ? var_symbol->type : node.type;
        const auto* type = lower_type(source_type, node.span);

        if (var_symbol == nullptr && type != nullptr) {
            const auto* function_type = type->as<FunctionType>();

            if (function_type != nullptr) {
                const auto* symbol = node.function_symbol;

                if (symbol == nullptr) {
                    diagnostics.add_error(node.span, "function value has no resolved semantic symbol");

                    return nullptr;
                }

                name = Mangler::function_name(symbol->base_name(), symbol, function_type);
            }
        }

        HIRExpression* value = nullptr;

        if (const auto captured = captured_variables.find(var_symbol); captured != captured_variables.end()) {
            const auto& [parameter, pointer_type] = captured->second;
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, parameter, pointer_type);
            value = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, pointer_type->element);
        } else {
            value = program->nodes.create<HIRIdentifierExpression>(
                node.span, name, type, var_symbol, node.function_symbol);
        }

        const auto* reference = type != nullptr ? type->as<PointerType>() : nullptr;

        if (reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            value = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, value, reference->element);
        }

        return value;
    }

    void reference_type(const std::string& name, const Type* type) {
        runtime_types.emplace(name, type);
    }

    template <typename Declaration>
    void register_nominal(Declaration& node) {
        if (!node.generic_parameters.empty()) {
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* nominal = lowered_type != nullptr ? lowered_type->as_nominal() : nullptr;

        if (nominal == nullptr || !resolver.is_concrete(lowered_type)) {
            return;
        }

        reference_type(type_name(*nominal), lowered_type);
    }

    void declare_types(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                register_nominal(*struct_declaration);
            }

            if (auto* interface_declaration = statement->as<InterfaceDeclaration>();
                interface_declaration != nullptr) {
                register_nominal(*interface_declaration);
            }

            if (auto* enum_declaration = statement->as<EnumDeclaration>();
                enum_declaration != nullptr) {
                register_nominal(*enum_declaration);
            }
        }
    }

    void drain_specializations() {
        while (true) {
            std::vector<HIRFunctionDeclaration*> pending;
            specializations.drain(pending);

            if (pending.empty()) {
                break;
            }

            for (auto* function : pending) {
                program->statements.push_back(function);
            }
        }
    }

  public:
    using Visitor<HIRNode*>::visit;
    Diagnostics& diagnostics;

    explicit Lowerer(Context& context, SemaContext& sema)
        : sema(sema), program(std::make_shared<HIRProgram>()),
          resolver(sema.types, sema.env, context.diagnostics),
          facades(sema.facades, resolver),
          construction(context.diagnostics, *program, sema, resolver, names, specializations,
              [this](const Type* type, Span span, Operation::Type operation) -> const Type* {
                  return prepare_lifecycle(type, span, operation);
              }),
          cleanup(*program, sema, resolver, construction, names),
          temporaries(*program, sema, cleanup, names),
          diagnostics(context.diagnostics) {}

    HIRNode* visit_expression(Expression& node) {
        return Visitor<HIRNode*>::visit_expression(node);
    }

    HIRNode* visit_statement(Statement& node) { return Visitor<HIRNode*>::visit_statement(node); }

    HIRNode* visit(TypeExpression& node) override {
        const auto* lowered = lower_type(node.type, node.span);

        return program->nodes.create<HIRTypeExpression>(node.span, lowered);
    }

    HIRNode* visit(NumberLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRNumberLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(FloatLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRFloatLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(StringLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRStringLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(CharLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRCharLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(BooleanLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRBooleanLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(NullLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRNullLiteral>(node.span, type);
    }

    HIRNode* visit(IdentifierExpression& node) override {
        if (node.compile_time_value.has_value()) {
            if (auto* lowered = lower_compile_time_value(node.span, *node.compile_time_value);
                lowered != nullptr) {
                return lowered;
            }
        }

        if (node.generic_declaration != nullptr) {
            const auto binding = resolver.const_parameter(node.name, node.generic_declaration);
            if (binding.has_value()) {
                if (!binding->value.has_value()) {
                    diagnostics.add_error(
                        node.span, "runtime expression requires a concrete const generic argument");

                    return nullptr;
                }
                return lower_compile_time_value(node.span, *binding->value);
            }
        }

        if (node.implicit_self_field) {
            const auto* self_symbol = sema.env.current_scope->lookup_var("self");
            const auto* self_type =
                self_symbol != nullptr ? lower_type(self_symbol->type, node.span) : nullptr;
            const auto* pointer_type =
                self_type != nullptr ? self_type->as<PointerType>() : nullptr;
            const auto* object_type = pointer_type != nullptr ? pointer_type->element : nullptr;

            auto* self = program->nodes.create<HIRIdentifierExpression>(node.span, "self",
                                                                                self_type);
            auto* object = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, self, object_type);

            const auto* enumeration = object_type != nullptr ? object_type->as<EnumType>() : nullptr;

            if (node.name == "value" && object_type != nullptr && !object_type->is<StructType>() &&
                (enumeration == nullptr || enumeration->find_field(node.name) == nullptr)) {
                return object;
            }

            return program->nodes.create<HIRMemberExpression>(
                node.span, object, node.name, lower_type(node.type, node.span));
        }

        return lower_identifier(node);
    }

    HIRNode* visit(BinaryExpression& node) override {
        auto* left = lower_expression(*node.left);
        auto* right = lower_expression(*node.right);

        if (node.op == BinaryOperator::Type::Is) {
            const auto* identifier = node.left->as<IdentifierExpression>();

            if (node.left->as<TypeExpression>() != nullptr ||
                (identifier != nullptr && identifier->var_symbol == nullptr)) {
                const auto* target = right->as<HIRTypeExpression>();

                return program->nodes.create<HIRBooleanLiteral>(node.span,
                    left->type->same(target->type_value), sema.primitives.entries.at("boolean"));
            }
        }
        const auto* type = node.op == BinaryExpression::Operator::Type::As && right != nullptr &&
                                   right->as<HIRTypeExpression>() != nullptr
                               ? right->as<HIRTypeExpression>()->type_value
                               : lower_type(node.type, node.span);

        return program->nodes.create<HIRBinaryExpression>(
            node.span, left, node.op, right, type);
    }

    HIRNode* visit(UnaryExpression& node) override {
        auto* operand = lower_expression(*node.operand);

        if (node.op == UnaryOperator::Type::AddressOf || node.op == UnaryOperator::Type::AddressOfMut) {
            operand = temporaries.capture(operand, node.span);
        }
        if (operand == nullptr) {
            return nullptr;
        }

        const auto* pointer_type =
            operand->type != nullptr ? operand->type->as<PointerType>() : nullptr;
        const auto* type =
            node.op == UnaryExpression::Operator::Type::Dereference && pointer_type != nullptr
                ? pointer_type->element
                : lower_type(node.type, node.span);

        return program->nodes.create<HIRUnaryExpression>(
            node.span, node.op, operand, type);
    }

    HIRNode* visit(CoerceExpression& node) override {
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);
        if (node.coercion == Coercion::Type::BaseSlice) {
            value = temporaries.capture(value, node.span);
            const auto* source_pointer_type = sema.types.create<PointerType>(value->type, false);
            const auto* pointer_type = sema.types.create<PointerType>(type, false);
            auto* address = program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::AddressOf,
                value, source_pointer_type);
            auto* pointer = program->nodes.create<HIRCoerceExpression>(node.span, address, pointer_type,
                Coercion::Type::None, source_pointer_type);
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, pointer, type);
        }
        if (node.coercion == Coercion::Type::CopyValue || node.coercion == Coercion::Type::MoveValue) {
            if (node.operation == nullptr) {
                diagnostics.add_error(node.span, "construction has no resolved lifecycle plan");

                return nullptr;
            }

            return construction.construct(node.span, value, node.operation);
        }
        const auto* source_type = lower_type(node.source_type, node.span);

        if (node.coercion == Coercion::Type::InterfaceValue) {
            value = temporaries.capture(value, node.span);
        }

        auto* result = program->nodes.create<HIRCoerceExpression>(node.span, value, type,
                                                                node.coercion, source_type);

        if (node.coercion == Coercion::Type::InterfaceValue &&
            node.implementations.size() != type->as<InterfaceType>()->methods.size()) {
            diagnostics.add_error(node.span, "interface conversion is missing resolved implementations");

            return nullptr;
        }

        result->implementations.reserve(node.implementations.size());

        for (const auto& method : node.implementations) {
            const auto* signature = lower_function_type(method.signature, node.span);
            auto name = Mangler::function_name(method.function_symbol->base_name(),
                                               method.function_symbol, signature);
            const auto* receiver = signature->parameters.front().type->as<PointerType>();
            const auto* owner = receiver != nullptr ? receiver->element->as_nominal() : nullptr;

            if (owner != nullptr && !owner->generic_arguments.empty()) {
                CallExpression request(node.span, node.value, {}, {});
                auto* callable = specialize_call(request, name, signature,
                    specializations.get_function(method.function_symbol),
                    lower_generic_arguments(owner->generic_arguments, node.span));
                const auto* identifier = callable->as<HIRIdentifierExpression>();
                name = identifier->name;
            }

            result->implementations.emplace_back(std::move(name), signature);
        }

        return result;
    }

    HIRNode* visit(BuiltinCall& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }

        if (node.name == "sizeof") {
            if (node.type_argument == nullptr || node.type_argument->type == nullptr) {
                return nullptr;
            }

            const auto* sized_type = lower_type(node.type_argument->source_type, node.span);
            auto layout = TypeLayout().resolve(sized_type);
            if (layout.state != TypeLayout::Result::State::Concrete &&
                node.type_argument->type != node.type_argument->source_type) {
                sized_type = lower_type(node.type_argument->type, node.span);
                layout = TypeLayout().resolve(sized_type);
            }

            if (layout.state != TypeLayout::Result::State::Concrete) {
                diagnostics.add_error(node.span, "sizeof requires a concrete sized type");

                return nullptr;
            }
            const auto* type = lower_type(node.type, node.span);

            return program->nodes.create<HIRNumberLiteral>(node.span, std::to_string(layout.size),
                                                                   type);
        }

        if (node.name == "length") {
            Evaluator evaluator(diagnostics, &sema.env, &resolver.substitutions);
            const auto result = evaluator.evaluate(node);

            return result.value.has_value() ? lower_compile_time_value(node.span, *result.value)
                                            : nullptr;
        }

        return nullptr;
    }

    HIRNode* visit(TryExpression& node) override {
        return lower_expression(*node.value);
    }

    HIRNode* visit(CatchExpression& node) override {
        StateGuard tail_guard(consuming_block_tail, true);
        std::vector<HIRCatchArm> arms;
        arms.reserve(node.arms.size());
        HIRExpression* value = nullptr;

        {
            StateGuard handler_guard(temporaries.handlers, temporaries.handlers);

            for (const auto& arm : node.arms) {
                arms.emplace_back(temporaries.protect(arm), arm.name,
                    lower_type(arm.annotation->type, arm.span), nullptr);
            }

            value = lower_expression(*node.value);
        }

        for (auto index = std::size_t{0}; index < node.arms.size(); ++index) {
            auto& source = node.arms[index];
            auto& arm = arms[index];
            if (!check_scope(source.scope, source.span)) {
                return nullptr;
            }

            ScopeGuard scope(sema.env.current_scope, source.scope);
            CleanupScopeGuard owned(cleanup);
            std::vector<HIRStatement*> statements;
            statements.reserve(4);

            if (auto* flag = cleanup.register_value(arm.name, arm.name, source.span, arm.type); flag != nullptr) {
                statements.push_back(flag);
            }

            auto* body = lower_statement(*source.body);

            if (terminates(body)) {
                statements.push_back(body);
            } else {
                const auto actions = cleanup.emit();
                auto* block = body->as<HIRBlockStatement>();

                if (block == nullptr) {
                    block = program->nodes.create<HIRBlockStatement>(source.span,
                        std::vector<HIRStatement*>{body});
                }

                auto* result = program->nodes.create<HIRBlockExpression>(source.span,
                    block, lower_type(source.body->type, source.span));
                auto* preserved = temporaries.finish(source.span, result, statements, actions);

                if (preserved != nullptr) {
                    statements.push_back(program->nodes.create<HIRExpressionStatement>(source.span, preserved));
                }
            }

            arm.body = program->nodes.create<HIRBlockStatement>(source.span, std::move(statements));
            arm.body->type = lower_type(source.body->type, source.span);
        }

        return program->nodes.create<HIRCatchExpression>(node.span, value,
            std::move(arms), lower_type(node.type, node.span));
    }

    HIRNode* visit(ThrowExpression& node) override {
        auto* value = lower_expression(*node.value);

        const auto decisions = sema.errors.targets.find(&node);

        if (decisions == sema.errors.targets.end() || decisions->second.size() != 1) {
            diagnostics.add_error(node.span, "throw has no resolved error target");

            return nullptr;
        }

        auto edge = temporaries.failure(lower_type(decisions->second.front().type, node.span),
                                        decisions->second.front().handler, node.span, diagnostics);

        if (!edge.has_value()) {
            return nullptr;
        }

        return program->nodes.create<HIRThrowExpression>(
            node.span, value, std::move(*edge), lower_type(node.type, node.span));
    }

    HIRNode* visit(CallExpression& node) override {
        if (node.destruction != nullptr) {
            const auto* member = node.callee->as<MemberExpression>();
            auto* value = lower_expression(*member->value);
            const auto* identifier = member->value->as<IdentifierExpression>();
            auto* action = cleanup.destroy(node.span,
                identifier != nullptr ? identifier->name : std::string(), value,
                lower_type(member->value->type, node.span), node.destruction);

            return program->nodes.create<HIRBlockExpression>(node.span,
                program->nodes.create<HIRBlockStatement>(node.span, std::vector<HIRStatement*>{action}),
                sema.primitives.entries.at("void"));
        }

        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }
        auto* call = lower_call(node);
        if (node.reference_result != nullptr && call != nullptr) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, call,
                lower_type(node.type, node.span));
        }
        return call;
    }

    HIRNode* visit(IndexExpression& node) override {
        auto* value = lower_expression(*node.value);
        value = temporaries.capture(value, node.span);
        auto* index = lower_expression(*node.index);
        const Type* type = nullptr;

        if (const auto* array_type =
                value != nullptr && value->type != nullptr ? value->type->as<ArrayType>() : nullptr;
            array_type != nullptr) {
            type = array_type->element;
        } else if (const auto* pointer_type = value != nullptr && value->type != nullptr
                                                  ? value->type->as<PointerType>()
                                                  : nullptr;
                   pointer_type != nullptr) {
            type = pointer_type->element;
        } else if (value != nullptr && value->type != nullptr && value->type->is<StringType>()) {
            type = sema.primitives.entries.at("char");
        } else {
            type = lower_type(node.type, node.span);
        }

        auto* result = program->nodes.create<HIRIndexExpression>(node.span, value, index, type);
        if (const auto* reference = type->as<PointerType>(); reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, result, reference->element);
        }
        return result;
    }

    HIRNode* visit(MemberExpression& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }
        auto* value = lower_expression(*node.value);
        value = temporaries.capture(value, node.span);
        const Type* type = nullptr;

        const auto* lowered_value_type = value != nullptr ? value->type : nullptr;
        if (const auto* pointer_type =
                lowered_value_type != nullptr ? lowered_value_type->as<PointerType>() : nullptr;
            pointer_type != nullptr) {
            lowered_value_type = pointer_type->element;
        }

        if (const auto* struct_type =
                lowered_value_type != nullptr ? lowered_value_type->as<StructType>() : nullptr;
            struct_type != nullptr) {
            if (const auto* field = struct_type->find_field(node.member); field != nullptr) {
                type = field->type;
            }
        }

        if (type == nullptr) {
            type = lower_type(node.type, node.span);
        }

        const auto* enum_type =
            node.value->type != nullptr ? node.value->type->as<EnumType>() : nullptr;
        if (enum_type != nullptr && enum_type->backing_type != nullptr && node.member == "value") {
            return program->nodes.create<HIRCoerceExpression>(
                node.span, value, type, Coercion::Type::None,
                lower_type(node.value->type, node.span));
        }

        if (node.member == "value" && facades.value(node.value->type) != nullptr) {
            return value;
        }

        auto* result = program->nodes.create<HIRMemberExpression>(node.span, value, node.member, type);
        if (const auto* reference = type->as<PointerType>(); reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, result, reference->element);
        }
        return result;
    }

    HIRNode* visit(QualifiedAccessExpression& node) override {
        if (node.variant_type != nullptr) {
            const auto* type = lower_type(node.type, node.span);
            const auto* enum_type = type != nullptr ? type->as<EnumType>() : nullptr;

            if (enum_type == nullptr) {
                return nullptr;
            }

            if (enum_type->backing_type != nullptr) {
                if (!node.enum_value.has_value()) {
                    return nullptr;
                }

                auto* value = lower_compile_time_value(node.span, *node.enum_value);
                return program->nodes.create<HIRCoerceExpression>(
                    node.span, value, type, Coercion::Type::None,
                    lower_type(node.enum_value->type, node.span));
            }

            return program->nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, node.member, std::vector<HIRStructLiteralField>{},
                node.variant_type->index, type);
        }

        if (node.function_symbol == nullptr) {
            return nullptr;
        }

        const auto* type = lower_type(node.type, node.span);
        auto name = Mangler::function_name(node.function_symbol->base_name(), node.function_symbol,
                                           type != nullptr ? type->as<FunctionType>() : nullptr);


        return program->nodes.create<HIRIdentifierExpression>(node.span, name, type, nullptr,
                                                              node.function_symbol);
    }

    HIRNode* visit(NewExpression& node) override {
        auto* pointer = lower_expression(*node.storage);
        const auto* pointer_type = lower_type(node.type, node.span)->as<PointerType>();
        pointer = program->nodes.create<HIRCoerceExpression>(
            node.span, pointer, pointer_type, Coercion::Type::None, pointer->type);

        auto name = names.next("placement");
        auto* captured = program->nodes.create<HIRIdentifierExpression>(node.span, name, pointer_type);
        auto* target = program->nodes.create<HIRUnaryExpression>(
            node.span, UnaryOperator::Type::Dereference, captured, pointer_type->element);

        std::vector<HIRStatement*> statements;
        statements.reserve(4);
        statements.push_back(program->nodes.create<HIRVarDeclaration>(node.span,
            Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            name, pointer_type, pointer, false));

        auto* value = lower_expression(*node.value);
        auto* assignment = program->nodes.create<HIRAssignExpression>(
            node.span, target, value, pointer_type->element);
        statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, assignment));

        const auto* address = node.storage->as<UnaryExpression>();
        const auto* local = address != nullptr && address->op == UnaryOperator::Type::AddressOfMut
            ? address->operand->as<IdentifierExpression>() : nullptr;
        const auto flag = local != nullptr ? cleanup.flag(local->name) : std::string();

        if (!flag.empty()) {
            const auto* boolean = sema.primitives.entries.at("boolean");
            auto* target = program->nodes.create<HIRIdentifierExpression>(node.span, flag, boolean);
            auto* initialized = program->nodes.create<HIRBooleanLiteral>(node.span, true, boolean);
            auto* rearm = program->nodes.create<HIRAssignExpression>(node.span, target, initialized, boolean);
            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, rearm));
        }

        statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, captured));

        return program->nodes.create<HIRBlockExpression>(node.span,
            program->nodes.create<HIRBlockStatement>(node.span, std::move(statements)), pointer_type);
    }

    HIRNode* visit(AssignExpression& node) override {

        if (node.resolved_assignment != nullptr) {
            return visit(*node.resolved_assignment);
        }
        auto* target = lower_expression(*node.target);
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);

        if (target != nullptr && type != nullptr &&
            sema.lifecycle.needs_drop(type, resolver)) {
            const auto* pointer_type = sema.types.create<PointerType>(type, true);
            const auto pointer_name = names.next("return_tmp");
            const auto value_name = names.next("return_tmp");
            auto* address = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::AddressOfMut, target, pointer_type);
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, pointer_name, pointer_type);
            auto* destination = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, type);
            auto* replacement = program->nodes.create<HIRIdentifierExpression>(node.span, value_name, type);
            std::vector<HIRStatement*> statements;
            statements.reserve(6);
            statements.push_back(program->nodes.create<HIRVarDeclaration>(
                node.span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                pointer_name, pointer_type, address, false));
            statements.push_back(program->nodes.create<HIRVarDeclaration>(
                node.span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                value_name, type, value, false));

            std::vector<HIRStatement*> drops;
            construction.destroy(node.span, destination,
                sema.lifecycle.resolve(type, Operation::Type::Destroy, resolver), drops);
            auto* drop_body = program->nodes.create<HIRBlockStatement>(node.span, std::move(drops));
            auto flag_name = std::string();
            if (const auto* identifier = node.target->as<IdentifierExpression>(); identifier != nullptr) {
                flag_name = cleanup.flag(identifier->name);
            }

            const auto* boolean_type = sema.primitives.entries.at("boolean");
            if (flag_name.empty()) {
                statements.push_back(drop_body);
            } else {
                auto* condition = program->nodes.create<HIRIdentifierExpression>(node.span, flag_name, boolean_type);
                auto* drop = program->nodes.create<HIRIfExpression>(
                    node.span, condition, drop_body, nullptr, sema.primitives.entries.at("void"));
                statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, drop));
            }

            auto* assignment = program->nodes.create<HIRAssignExpression>(node.span, destination, replacement, type);
            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, assignment));
            if (!flag_name.empty()) {
                auto* flag = program->nodes.create<HIRIdentifierExpression>(node.span, flag_name, boolean_type);
                auto* initialized = program->nodes.create<HIRBooleanLiteral>(node.span, true, boolean_type);
                auto* rearm = program->nodes.create<HIRAssignExpression>(node.span, flag, initialized, boolean_type);
                statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, rearm));
            }

            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, destination));
            return program->nodes.create<HIRBlockExpression>(
                node.span, program->nodes.create<HIRBlockStatement>(node.span, std::move(statements)), type);
        }

        return program->nodes.create<HIRAssignExpression>(node.span, target, value, type);
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        if (facades.backing(node.type) != nullptr) {
            for (auto* field : node.fields) {
                if (field->name == "value") {
                    return lower_expression(*field->value);
                }
            }
        }

        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            auto* qualified = node.name->as<QualifiedAccessExpression>();
            if (qualified == nullptr || qualified->variant_type == nullptr) {
                return nullptr;
            }

            std::vector<HIRStatement*> transfers;
            transfers.reserve(node.fields.size());
            auto fields = lower_fields(node.fields, transfers);

            auto* value = program->nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, qualified->member, std::move(fields),
                qualified->variant_type->index, type);

            return temporaries.transfer(value, transfers);
        }

        const auto* struct_type = type->as<StructType>();
        if (struct_type == nullptr) {
            return nullptr;
        }

        std::vector<HIRStatement*> transfers;
        transfers.reserve(node.fields.size());
        auto fields = lower_fields(node.fields, transfers);

        auto* value = program->nodes.create<HIRStructLiteralExpression>(
            node.span, struct_type->name, std::move(fields), type);

        return temporaries.transfer(value, transfers);
    }

    HIRNode* visit(EnumVariantExpression& node) override {
        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        const auto* enum_type = type->as<EnumType>();
        if (enum_type == nullptr || node.variant_type == nullptr) {
            return nullptr;
        }

        std::vector<HIRStructLiteralField> fields;
        fields.reserve(node.fields.size());
        std::vector<HIRStatement*> transfers;
        transfers.reserve(node.fields.size());

        const auto* variant = enum_type->find_variant(node.variant_name);

        for (const auto& declared : enum_type->fields_for(*variant)) {
            const auto field = std::ranges::find(node.fields, declared.name,
                [](const auto* entry) -> const std::string& { return entry->name; });

            if (field != node.fields.end()) {
                fields.emplace_back(declared.name,
                    temporaries.capture(lower_expression(*(*field)->value), (*field)->span, &transfers));
            }
        }

        auto* value = program->nodes.create<HIREnumVariantExpression>(
            node.span, enum_type->name, node.variant_name, std::move(fields),
            node.variant_type->index, type);

        return temporaries.transfer(value, transfers);
    }

    HIRNode* visit(IfExpression& node) override {
        auto* condition = lower_condition(*node.condition);
        StateGuard tail_guard(consuming_block_tail, true);
        auto* then_branch = lower_statement(*node.then_branch);
        auto* else_branch = node.else_branch != nullptr ? lower_statement(*node.else_branch) : nullptr;

        return program->nodes.create<HIRIfExpression>(
            node.span, condition, then_branch, else_branch, lower_type(node.type, node.span));
    }

    HIRNode* visit(WhenArm& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(WhenExpression& node) override {
        StateGuard tail_guard(consuming_block_tail, true);
        auto* subject = node.subject != nullptr ? lower_expression(*node.subject) : nullptr;
        subject = temporaries.capture(subject, node.span);
        const auto* type = lower_type(node.type, node.span);

        std::vector<HIRWhenArm> arms;
        arms.reserve(node.arms.size());

        for (auto* arm : node.arms) {
            if (!check_scope(arm->scope, arm->span)) {
                return nullptr;
            }

            ScopeGuard scope(sema.env.current_scope, arm->scope);
            std::vector<HIRWhenPattern> patterns;
            patterns.reserve(arm->patterns.size());

            for (auto* pattern : arm->patterns) {
                if (pattern->pattern_kind == WhenPattern::PatternKind::Type::Expression) {
                    patterns.emplace_back(lower_expression(*pattern->expression));
                    continue;
                }

                std::vector<HIRWhenPatternField> fields;
                fields.reserve(pattern->fields.size());

                if (pattern->enum_type != nullptr && pattern->variant_type != nullptr) {
                    for (auto* field : pattern->fields) {
                        const auto* declared = pattern->enum_type->find_field(field->field_name);

                        if (declared == nullptr) {
                            declared = pattern->variant_type->find_field(field->field_name);
                        }

                        if (declared == nullptr) {
                            diagnostics.add_error(field->span, "enum pattern has no checked field");

                            return nullptr;
                        }

                        fields.emplace_back(field->field_name, field->binding_name,
                                            lower_type(declared->type, field->span));
                    }
                }

                const auto* enumeration = pattern->enum_type;
                const auto* variant = pattern->variant_type;
                HIRExpression* value = nullptr;

                if (enumeration != nullptr && enumeration->backing_type != nullptr &&
                    variant != nullptr && variant->discriminant.has_value()) {
                    value = lower_compile_time_value(pattern->span, *variant->discriminant);
                }

                patterns.emplace_back(enumeration != nullptr ? enumeration->name : std::string(),
                    pattern->variant_name, variant != nullptr ? variant->index : 0,
                    value, std::move(fields));
            }

            auto* guard = arm->guard != nullptr ? lower_expression(*arm->guard) : nullptr;
            auto* body = lower_statement(*arm->body);
            arms.emplace_back(arm->is_else, std::move(patterns), guard, body);
        }

        return program->nodes.create<HIRWhenExpression>(
            node.span, subject, std::move(arms), node.is_exhaustive, type);
    }

    HIRNode* visit(WhileStatement& node) override {
        auto* condition = lower_condition(*node.condition);
        auto* body = lower_statement(*node.body);

        return program->nodes.create<HIRLoopStatement>(node.span, std::vector<HIRStatement*>(),
            condition, nullptr, body, lower_type(node.type, node.span));
    }

    HIRNode* visit(ForStatement& node) override {
        if (!check_scope(node.scope, node.span)) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, node.scope);
        std::vector<HIRStatement*> initializers;
        initializers.reserve(1);

        if (auto* initializer = lower_statement(*node.initializer); initializer != nullptr) {
            initializers.push_back(initializer);
        }

        auto* condition = lower_condition(*node.condition);
        auto* step = node.step != nullptr ? lower_condition(*node.step) : nullptr;
        auto* body = lower_statement(*node.body);

        return program->nodes.create<HIRLoopStatement>(node.span, std::move(initializers),
            condition, step, body, lower_type(node.type, node.span));
    }

    HIRNode* visit(ClosureExpression& node) override {
        auto name = names.next("closure");
        const auto* type_base = lower_type(node.type, node.span);
        const auto* type = type_base != nullptr ? type_base->as<FunctionType>() : nullptr;

        if (type == nullptr) {
            return nullptr;
        }

        if (!check_scope(node.body->scope, node.span)) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, node.body->scope->parent);

        auto* body = build_owned_body(node.parameters, type, *node.body);

        auto* function = program->nodes.create<HIRFunctionDeclaration>(
            node.span, Visibility::Type::Private, Linkage::Type::Internal, name, type->return_type,
            body, false, type, false);

        program->statements.push_back(function);

        return program->nodes.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BlockExpression& node) override {
        StateGuard tail_guard(consuming_block_tail, true);
        auto* body = lower_block(*node.body);
        return program->nodes.create<HIRBlockExpression>(node.span, body,
                                                                 lower_type(node.type, node.span));
    }

    HIRNode* visit(ArrayLiteralExpression& node) override {
        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        std::vector<HIRExpression*> elements;
        elements.reserve(node.elements.size());
        std::vector<HIRStatement*> transfers;
        transfers.reserve(node.elements.size());
        for (auto* element : node.elements) {
            auto* lowered = lower_expression(*element);
            if (lowered == nullptr) {
                return nullptr;
            }

            elements.push_back(temporaries.capture(lowered, element->span, &transfers));
        }

        auto* value = program->nodes.create<HIRArrayLiteralExpression>(node.span, std::move(elements), type);

        return temporaries.transfer(value, transfers);
    }

    HIRNode* visit(BlockStatement& node) override {
        const auto consumes_tail = consuming_block_tail;
        StateGuard tail_guard(consuming_block_tail, false);
        if (!check_scope(node.scope, node.span)) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, node.scope);
        CleanupScopeGuard cleanup_guard(cleanup);

        std::vector<HIRStatement*> body;
        body.reserve(node.statements.size());
        auto terminated = false;

        for (auto index = std::size_t{0}; index < node.statements.size(); ++index) {
            if (diagnostics.has_errors()) {
                return nullptr;
            }

            auto* statement = node.statements[index];
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                const auto* source_expression = statement->as<ExpressionStatement>();
                const auto* source_call = source_expression != nullptr ? source_expression->expression->as<CallExpression>() : nullptr;
                const auto owns_result = source_expression != nullptr &&
                    ((source_call != nullptr && source_call->reference_result == nullptr) ||
                     source_expression->expression->as<BlockExpression>() != nullptr ||
                     source_expression->expression->as<IfExpression>() != nullptr ||
                     source_expression->expression->as<WhenExpression>() != nullptr ||
                     source_expression->expression->as<TryExpression>() != nullptr ||
                     source_expression->expression->as<CatchExpression>() != nullptr ||
                     source_expression->expression->as<StructLiteralExpression>() != nullptr ||
                     source_expression->expression->as<EnumVariantExpression>() != nullptr ||
                     source_expression->expression->as<CoerceExpression>() != nullptr ||
                     source_expression->expression->as<ArrayLiteralExpression>() != nullptr);
                if (auto* expression_statement = lowered->as<HIRExpressionStatement>();
                    (index + 1 < node.statements.size() || !consumes_tail) && owns_result && expression_statement != nullptr &&
                    expression_statement->expression != nullptr &&
                    sema.lifecycle.needs_drop(expression_statement->expression->type,
                                                    resolver)) {
                    auto* expression = expression_statement->expression;
                    auto name = names.next("return_tmp");
                    auto* temporary = program->nodes.create<HIRVarDeclaration>(
                        statement->span, Visibility::Type::Private, Linkage::Type::Internal,
                        StorageKind::Type::Var, name, expression->type, expression, false);
                    auto* value = program->nodes.create<HIRIdentifierExpression>(
                        statement->span, name, expression->type);
                    std::vector<HIRStatement*> statements;
                    statements.reserve(2);
                    statements.push_back(temporary);
                    construction.destroy(statement->span, value,
                        sema.lifecycle.resolve(expression->type, Operation::Type::Destroy, resolver), statements);
                    lowered = program->nodes.create<HIRStatementGroup>(
                        statement->span, std::move(statements));
                }

                body.push_back(lowered);

                if (terminates(lowered)) {
                    terminated = true;
                    break;
                }
            }
        }

        if (!terminated) {
            auto scope_destruction = cleanup.emit();
            HIRExpression* tail_value = nullptr;
            if (consumes_tail && !body.empty() && !scope_destruction.empty()) {
                auto* tail = body.back()->as<HIRExpressionStatement>();
                if (tail != nullptr && tail->expression != nullptr && tail->expression->type != nullptr &&
                    !tail->expression->type->is<VoidType>() && !tail->expression->type->is<NeverType>()) {
                    auto name = names.next("return_tmp");
                    auto* expression = tail->expression;
                    body.back() = program->nodes.create<HIRVarDeclaration>(node.span,
                        Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                        name, expression->type, expression, false);
                    tail_value = program->nodes.create<HIRIdentifierExpression>(node.span, name, expression->type);
                }
            }
            body.insert(body.end(), scope_destruction.begin(), scope_destruction.end());
            if (tail_value != nullptr) {
                body.push_back(program->nodes.create<HIRExpressionStatement>(node.span, tail_value));
            }
        }

        auto* result = program->nodes.create<HIRBlockStatement>(node.span, std::move(body));
        result->type = lower_type(node.type, node.span);
        return result;
    }

    HIRNode* visit(ExpressionStatement& node) override {
        auto* expression = lower_expression(*node.expression);

        return program->nodes.create<HIRExpressionStatement>(node.span, expression);
    }

    HIRNode* visit(ReturnStatement& node) override {
        auto* value = node.value != nullptr ? lower_expression(*node.value) : nullptr;

        auto cleanup_statements = temporaries.unwind(0);
        if (cleanup_statements.empty()) {
            return program->nodes.create<HIRReturnStatement>(node.span, value);
        }

        std::vector<HIRStatement*> statements;
        statements.reserve(cleanup_statements.size() + 2);
        value = temporaries.finish(node.span, value, statements, cleanup_statements);
        statements.push_back(program->nodes.create<HIRReturnStatement>(node.span, value));

        return program->nodes.create<HIRBlockStatement>(node.span, std::move(statements));
    }

    HIRNode* visit(DeferStatement& node) override {
        auto* body = lower_statement(*node.body);
        cleanup.defer(body);

        return nullptr;
    }

    HIRNode* visit(VarDeclaration& node) override {
        const auto original_drop_count = temporaries.current != nullptr ? temporaries.current->drops.size() : 0;
        HIRExpression* initializer = nullptr;
        if (node.initializer != nullptr) {
            if (sema.env.current_scope->is_global() &&
                node.initializer->compile_time_value.has_value()) {
                initializer = lower_compile_time_value(
                    node.span, *node.initializer->compile_time_value);
            } else {
                initializer = lower_expression(*node.initializer);
            }

            if (sema.env.current_scope->is_global() && temporaries.current != nullptr) {
                initializer = temporaries.complete(node.span, initializer, *temporaries.current);
            }
        }
        const auto* source_type = node.type != nullptr     ? node.type
                                  : initializer != nullptr ? initializer->type
                                                           : nullptr;
        const auto* type = lower_type(source_type, node.span);
        const auto* reference = type != nullptr ? type->as<PointerType>() : nullptr;
        if (temporaries.current != nullptr && reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            const auto added_count = temporaries.current->drops.size() - original_drop_count;
            for (auto index = added_count; index > 0; --index) {
                cleanup.defer(temporaries.current->drops[index - 1]);
            }
            temporaries.current->drops.erase(temporaries.current->drops.begin(), temporaries.current->drops.begin() + static_cast<std::ptrdiff_t>(added_count));
        }

        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        auto* declaration = program->nodes.create<HIRVarDeclaration>(
            node.span, node.visibility,
            symbol != nullptr && symbol->find_attribute(".test_name") != nullptr
                ? Linkage::Type::External : Linkage::Type::Internal,
            node.storage_kind, std::move(name),
            type, initializer, sema.env.current_scope->is_global(),
            AttributeResolver::convert(node.attributes));

        declaration->is_definition = true;

        if (auto* flag = cleanup.register_value(node.name, declaration->name, node.span, type); flag != nullptr) {
            std::vector<HIRStatement*> statements;
            statements.reserve(2);
            statements.push_back(declaration);
            statements.push_back(flag);

            return program->nodes.create<HIRStatementGroup>(node.span,
                                                                    std::move(statements));
        }

        return declaration;
    }

    HIRNode* visit(ExternVarDeclaration& node) override {
        const auto* type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        return program->nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, Linkage::Type::External, StorageKind::Type::Var,
            std::move(name), type, nullptr, true, AttributeResolver::convert(node.attributes));
    }

    HIRNode* visit(TestDeclaration& node) override {
        return node.enabled ? visit(*node.function) : nullptr;
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (node.is_deleted) {
            return nullptr;
        }
        if (!node.prototype->generic_parameters.empty()) {
            return nullptr;
        }

        if (!check_scope(node.scope, node.span)) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, node.scope);

        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);
        if (type == nullptr || !is_emittable_function_type(type)) {
            return nullptr;
        }

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto linkage = Mangler::function_linkage(symbol);
        auto mangled = Mangler::is_mangled(symbol);


        auto* body = build_body(node, type);

        if (body == nullptr) {
            return nullptr;
        }

        return program->nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, linkage, std::move(name), return_type, body, variadic, type,
            mangled, Abi::Type::Language, symbol);
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto mangled = Mangler::is_mangled(symbol);

        return program->nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, std::move(name), return_type,
            nullptr, variadic, type, mangled, Abi::Type::C, symbol);
    }

    HIRNode* visit(StructDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        for (auto* nested_struct : node.structs) {
            visit(*nested_struct);
        }

        for (auto* nested_enum : node.enums) {
            visit(*nested_enum);
        }

        return nullptr;
    }

    HIRNode* visit(EnumDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        return nullptr;
    }

    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Field& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(EnumVariant& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPatternField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPattern& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Attribute& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(ImportStatement& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(InterfaceDeclaration& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(TypeAliasDeclaration& node [[maybe_unused]]) override { return nullptr; }

    void register_generic_declarations(const std::vector<const Program*>& programs) {
        for (const auto* program : programs) {
            if (program == nullptr) {
                continue;
            }

            for (auto* statement : program->statements) {
                std::span<FunctionDeclaration* const> functions;
                auto* function = statement->as<FunctionDeclaration>();

                if (auto* structure = statement->as<StructDeclaration>(); structure != nullptr) {
                    if (!structure->generic_parameters.empty()) {
                        specializations.register_struct(structure);
                    }

                    functions = structure->methods;
                } else if (auto* enumeration = statement->as<EnumDeclaration>(); enumeration != nullptr) {
                    functions = enumeration->methods;
                } else if (function != nullptr) {
                    functions = std::span<FunctionDeclaration* const>(&function, 1);
                }

                for (const auto* declaration : functions) {
                    const auto* symbol = declaration->function_symbol;

                    if (symbol == nullptr || symbol->signature() == nullptr) {
                        diagnostics.add_error(declaration->span, "function has no checked signature");

                        return;
                    }

                    if (!symbol->signature()->generic_parameters.empty()) {
                        specializations.register_function(symbol->base_name(), declaration, symbol);
                    }
                }
            }
        }
    }

    std::shared_ptr<HIRProgram> lower_module(Program& program_node, Scope* module_scope,
                                             const std::vector<const Program*>& all_programs) {
        ScopeGuard module_guard(sema.env.current_scope,
            module_scope != nullptr ? module_scope : sema.env.root_scope);

        register_generic_declarations(all_programs);

        if (diagnostics.has_errors()) {
            return nullptr;
        }

        declare_types(program_node);

        if (module_scope != nullptr && module_scope->parent != nullptr &&
            module_scope->parent->kind == Scope::Kind::Type::Module) {
            for (const auto& [name, symbol] : module_scope->parent->local_variables()) {
                if (symbol->find_attribute(".test_name") == nullptr) {
                    continue;
                }

                program->statements.push_back(program->nodes.create<HIRVarDeclaration>(
                    symbol->span, symbol->visibility, Linkage::Type::External,
                    symbol->storage_kind, Mangler::linker_name(name, symbol),
                    lower_type(symbol->type, symbol->span), nullptr, true));
            }
        }

        for (auto& statement : program_node.statements) {
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                program->statements.push_back(lowered);
            }

            if (diagnostics.has_errors()) {
                return nullptr;
            }
        }

        std::vector<HIRStatement*> global_initialization;
        global_initialization.reserve(program->statements.size());

        for (auto* statement : program->statements) {
            auto* variable = statement->as<HIRVarDeclaration>();

            if (variable != nullptr && variable->is_global && variable->initializer != nullptr) {
                global_initialization.push_back(variable);
            }
        }

        std::vector<HIRStatement*> global_destruction;
        global_destruction.reserve(global_initialization.size());
        for (auto iterator = program->statements.rbegin(); iterator != program->statements.rend();
             ++iterator) {
            auto* statement = *iterator;
            auto* variable = statement->as<HIRVarDeclaration>();
            if (variable == nullptr || !variable->is_global || variable->initializer == nullptr ||
                !sema.lifecycle.needs_drop(variable->type, resolver)) {
                continue;
            }

            auto* value = program->nodes.create<HIRIdentifierExpression>(
                variable->span, variable->name, variable->type);
            construction.destroy(variable->span, value,
                sema.lifecycle.resolve(variable->type, Operation::Type::Destroy, resolver), global_destruction);
        }

        for (const auto role : {HIRFunctionDeclaration::Role::Type::GlobalInitializer,
                                HIRFunctionDeclaration::Role::Type::GlobalDestructor}) {
            auto& statements = role == HIRFunctionDeclaration::Role::Type::GlobalInitializer
                ? global_initialization : global_destruction;

            if (statements.empty()) {
                continue;
            }

            const auto* void_type = sema.primitives.entries.at("void");
            const auto* function_type = sema.types.create<FunctionType>(
                std::string(), void_type, std::vector<ParameterType>(),
                std::vector<GenericParameterType>(), false);
            auto name = std::string(role == HIRFunctionDeclaration::Role::Type::GlobalInitializer
                ? ".zep.global_init." : ".zep.global_dtor.") +
                        (module_scope != nullptr ? module_scope->name : std::string("module"));
            auto* body = program->nodes.create<HIRBlockStatement>(
                Span(), std::move(statements));
            auto* function = program->nodes.create<HIRFunctionDeclaration>(
                Span(), Visibility::Type::Private, Linkage::Type::Internal, std::move(name),
                void_type, body, false, function_type, false, Abi::Type::Language, nullptr);
            function->role = role;
            program->statements.push_back(function);
        }

        drain_specializations();

        program->referenced_types.reserve(runtime_types.size());

        for (const auto& [name, type] : runtime_types) {
            program->referenced_types.emplace_back(name, type);
        }

        return diagnostics.has_errors() ? nullptr : program;
    }

    std::shared_ptr<HIRProgram> lower(Program& program_node) {
        std::vector<const Program*> programs{&program_node};
        return lower_module(program_node, sema.env.root_scope, programs);
    }
};
