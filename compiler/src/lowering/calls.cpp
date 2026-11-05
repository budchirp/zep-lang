module;

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

module zep.compiler.lowering;
import zep.common.state;
import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.compiler.lowering.mangler;
import zep.hir.node;
import zep.hir.program;

HIRCallExpression* Lowerer::make_call(CallExpression& node,
    std::unique_ptr<HIRCallTarget> target, std::vector<HIRExpression*> arguments) {
    const auto* result = node.reference_result != nullptr ? node.reference_result : node.type;

    return program->nodes.create<HIRCallExpression>(
        node.span, std::move(target), std::move(arguments), lower_type(result, node.span));
}

HIRCallExpression* Lowerer::bind_call(CallExpression& node, HIRExpression* callee,
    std::vector<HIRExpression*> arguments) {
    if (node.resolved_target == nullptr) {
        throw LoweringFailure(node.span, "call has no resolved semantic target");
    }

    if (const auto* direct = node.resolved_target->as<DirectCallTarget>(); direct != nullptr) {
        const auto* identifier = callee != nullptr ? callee->as<HIRIdentifierExpression>() : nullptr;

        if (identifier == nullptr || identifier->name.empty()) {
            throw LoweringFailure(node.span, "direct call has no lowered callable");
        }

        const auto* function = identifier->function_symbol != nullptr
            ? identifier->function_symbol : direct->function_symbol;
        const auto* signature = identifier->type != nullptr
            ? identifier->type->as<FunctionType>() : lower_function_type(direct->signature, node.span);

        return make_call(node, std::make_unique<HIRDirectCallTarget>(*function,
            identifier->name, signature), std::move(arguments));
    }

    if (const auto* indirect = node.resolved_target->as<IndirectCallTarget>(); indirect != nullptr) {
        if (callee == nullptr) {
            return nullptr;
        }

        const auto* signature = lower_function_type(indirect->function_type, node.span);

        return make_call(node, std::make_unique<HIRIndirectCallTarget>(callee, *signature),
                          std::move(arguments));
    }

    if (const auto* dispatch = node.resolved_target->as<InterfaceCallTarget>(); dispatch != nullptr) {
        return make_call(node, std::make_unique<HIRInterfaceCallTarget>(
            *dispatch->method_symbol, dispatch->slot), std::move(arguments));
    }

    throw LoweringFailure(node.span, "call has no resolved semantic target");
}

HIRExpression* Lowerer::specialize_call(CallExpression& node, const std::string& name,
    const FunctionType* signature, const FunctionDeclaration* definition,
    const std::vector<GenericBinding>& bindings) {
    for (const auto& binding : bindings) {
        if (!is_concrete_binding(binding)) {
            throw LoweringFailure(node.span, "generic specialization requires concrete bindings");
        }
    }

    const auto identity = definition != nullptr ? std::variant<const Node*, const Type*>(definition)
        : std::variant<const Node*, const Type*>(signature);
    const auto instance = specializations.get_or_create(identity, name, bindings);

    if (!instance.is_generated && definition != nullptr) {
        auto* body = specialize(*definition, instance.name, bindings);

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

    return program->nodes.create<HIRIdentifierExpression>(node.callee->span, instance.name, type);
}

HIRCallExpression* Lowerer::lower_captured_call(CallExpression& node, ClosureExpression& closure) {
    const auto* signature = closure.type != nullptr ? closure.type->as<FunctionType>() : nullptr;

    if (signature == nullptr || signature->parameters.size() != node.arguments.size()) {
        return nullptr;
    }

    auto name = names.next("closure");
    std::vector<ParameterType> parameters;
    parameters.reserve(closure.captures.size() + signature->parameters.size());

    std::vector<HIRExpression*> arguments;
    arguments.reserve(closure.captures.size() + node.arguments.size());

    std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>> captures;
    captures.reserve(closure.captures.size());

    for (auto index = std::size_t{0}; index < closure.captures.size(); ++index) {
        const auto* symbol = sema.env.current_scope->lookup_var(closure.captures[index]);

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
        std::vector<GenericParameterType>(), false);
    auto* function = sema.env.symbols.create<FunctionSymbol>(
        name, node.span, Visibility::Type::Private, Linkage::Type::Internal, function_type);
    HIRBlockStatement* body = nullptr;

    {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes,
                         Scope::Kind::Type::Function, "captured_closure");

        for (auto index = std::size_t{0}; index < closure.parameters.size(); ++index) {
            const auto* parameter = closure.parameters[index];
            const auto* type = function_type->parameters[index + closure.captures.size()].type;
            auto* symbol = sema.env.symbols.create<VariableSymbol>(parameter->name,
                parameter->span, Visibility::Type::Private, StorageKind::Type::Var, type);

            sema.env.current_scope->define_var(parameter->name, symbol);
        }

        StateGuard capture_guard(captured_variables, std::move(captures));
        body = build_owned_body(closure.parameters, function_type, *closure.body, closure.captures.size());
    }

    if (body == nullptr) {
        return nullptr;
    }

    program->statements.push_back(program->nodes.create<HIRFunctionDeclaration>(node.span,
        Visibility::Type::Private, Linkage::Type::Internal, name, function_type->return_type,
        body, false, function_type, false, Abi::Type::Language, function));

    for (const auto* argument : node.arguments) {
        auto* value = lower_expression(*argument->value);

        if (value == nullptr) {
            return nullptr;
        }

        arguments.push_back(value);
    }

    return make_call(node, std::make_unique<HIRDirectCallTarget>(*function, name, function_type),
                      std::move(arguments));
}

HIRCallExpression* Lowerer::lower_call(CallExpression& node,
    std::optional<std::vector<HIRExpression*>> bound) {
    if (auto* closure = node.callee->as<ClosureExpression>();
        closure != nullptr && !closure->captures.empty()) {
        return lower_captured_call(node, *closure);
    }

    if (node.resolved_target == nullptr) {
        throw LoweringFailure(node.span, "call has no resolved semantic target");
    }

    std::vector<HIRExpression*> arguments;

    if (bound.has_value()) {
        arguments = std::move(*bound);
    } else {
        arguments.reserve(node.arguments.size());

        for (const auto* argument : node.arguments) {
            arguments.push_back(lower_expression(*argument->value));
        }
    }

    const auto* target = node.resolved_target->as<DirectCallTarget>();
    const auto* function = target != nullptr ? target->function_symbol : nullptr;

    if (function == nullptr) {
        const auto* signature = node.callee->type != nullptr ? node.callee->type->as<FunctionType>() : nullptr;
        auto* callee = signature == nullptr || node.generic_arguments.empty()
            ? lower_expression(*node.callee)
            : specialize_call(node, signature->name, signature,
                specializations.get_function(signature->name),
                lower_generic_arguments(node.generic_arguments, node.span));

        return bind_call(node, callee, std::move(arguments));
    }

    const auto* signature = function->signature();

    if (signature == nullptr) {
        return nullptr;
    }

    const auto* owner = node.type != nullptr ? node.type->as_nominal() : nullptr;
    const auto construction = function->constructor();
    std::vector<GenericBinding> bindings;

    if (construction && owner != nullptr && !owner->generic_arguments.empty()) {
        bindings = lower_generic_arguments(owner->generic_arguments, node.span);
    } else if (!node.generic_arguments.empty()) {
        bindings = lower_generic_arguments(node.generic_arguments, node.span);
    } else if (!node.arguments.empty() && !signature->generic_parameters.empty()) {
        const auto* receiver = node.arguments.front()->value->type;

        if (const auto* pointer = receiver != nullptr ? receiver->as<PointerType>() : nullptr;
            pointer != nullptr) {
            receiver = pointer->element;
        }

        if (const auto* nominal = receiver != nullptr ? receiver->as_nominal() : nullptr;
            nominal != nullptr) {
            bindings = lower_generic_arguments(nominal->generic_arguments, node.span);
        }
    }

    const auto name = Mangler::function_name(function->base_name(), function, signature);

    if (!bindings.empty()) {
        auto* callee = specialize_call(
            node, name, signature, specializations.get_function(function), bindings);

        return bind_call(node, callee, std::move(arguments));
    }

    const auto* direct = node.resolved_target->as<DirectCallTarget>();
    const auto* resolved = lower_function_type(direct != nullptr ? direct->signature : signature, node.span);
    auto emitted = Mangler::function_name(function->base_name(), function, resolved);
    register_function_symbol(emitted, function->visibility, Linkage::Type::External,
                             resolved, node.span, function, function->abi);

    auto* callee = program->nodes.create<HIRIdentifierExpression>(node.callee->span, emitted, resolved);

    return bind_call(node, callee, std::move(arguments));
}
