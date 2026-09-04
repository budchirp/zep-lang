module;

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.frontend.sema.resolver.call;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.resolver.member;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.coercion;
import zep.frontend.sema.type.resolver;

export class CallResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    FacadeResolver facades;
    MemberResolver member_resolver;
    Visitor<void>& visitor;
    const std::string& current_parent;
    const Type*& target_type;

    static void infer_type_match(const Type* declared, const Type* actual,
                                 std::unordered_map<std::string, const Type*>& inferred) {
        if (declared == nullptr || actual == nullptr) {
            return;
        }

        if (const auto* named = declared->as<NamedType>(); named != nullptr) {
            if (named->generic_arguments.empty()) {
                inferred.insert_or_assign(named->name, actual);
            }
            return;
        }

        if (const auto* declared_pointer = declared->as<PointerType>();
            declared_pointer != nullptr) {
            if (const auto* actual_pointer = actual->as<PointerType>(); actual_pointer != nullptr) {
                infer_type_match(declared_pointer->element, actual_pointer->element, inferred);
            }
            return;
        }

        if (const auto* declared_array = declared->as<ArrayType>(); declared_array != nullptr) {
            if (const auto* actual_array = actual->as<ArrayType>(); actual_array != nullptr) {
                infer_type_match(declared_array->element, actual_array->element, inferred);
            }
            return;
        }

        if (const auto* declared_func = declared->as<FunctionType>(); declared_func != nullptr) {
            if (const auto* actual_func = actual->as<FunctionType>(); actual_func != nullptr) {
                infer_type_match(declared_func->return_type, actual_func->return_type, inferred);
                const auto count =
                    std::min(declared_func->parameters.size(), actual_func->parameters.size());
                for (std::size_t i = 0; i < count; ++i) {
                    infer_type_match(declared_func->parameters[i].type,
                                     actual_func->parameters[i].type, inferred);
                }
            }
        }
    }

    std::vector<GenericArgument*>
    infer_from_arguments(const std::vector<GenericParameterType>& parameters,
                         const std::vector<ParameterType>& declared_parameters,
                         const std::vector<Argument*>& arguments, Span span) {
        std::unordered_map<std::string, const Type*> inferred;
        inferred.reserve(parameters.size());

        const auto count = std::min(declared_parameters.size(), arguments.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (arguments[i]->value != nullptr && arguments[i]->value->type != nullptr) {
                infer_type_match(declared_parameters[i].type, arguments[i]->value->type, inferred);
            }
        }

        if (inferred.size() != parameters.size()) {
            return {};
        }

        std::vector<GenericArgument*> generic_arguments;
        generic_arguments.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            auto iterator = inferred.find(parameter.name);
            if (iterator == inferred.end() || iterator->second == nullptr) {
                return {};
            }

            auto* type_expr = sema.nodes.create<TypeExpression>(span, iterator->second);
            generic_arguments.push_back(
                sema.nodes.create<GenericArgument>(span, parameter.name, type_expr));
        }

        return generic_arguments;
    }

    bool reorder_generic_arguments(std::vector<GenericArgument*>& arguments,
                                   const std::vector<GenericParameterType>& parameters, Span span,
                                   bool emit_errors) {
        if (arguments.size() != parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "expected " + std::to_string(parameters.size()) +
                              " generic argument(s), got " + std::to_string(arguments.size()));
            }
            return false;
        }

        std::vector<GenericArgument*> ordered(parameters.size(), nullptr);
        std::unordered_set<std::string> used_names;
        used_names.reserve(arguments.size());

        bool seen_named = false;
        bool valid = true;
        std::size_t positional_index = 0;

        for (auto* argument : arguments) {
            if (argument->name.empty()) {
                if (seen_named) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            argument->span,
                            "positional generic argument cannot follow named generic argument");
                    }
                    valid = false;
                    continue;
                }

                ordered[positional_index] = argument;
                used_names.insert(parameters[positional_index].name);
                ++positional_index;
                continue;
            }

            seen_named = true;

            std::size_t parameter_index = 0;
            for (; parameter_index < parameters.size(); ++parameter_index) {
                if (parameters[parameter_index].name == argument->name) {
                    break;
                }
            }

            if (parameter_index == parameters.size()) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "unknown generic argument '" +
                                                                      argument->name + "'");
                }
                valid = false;
                continue;
            }

            if (!used_names.insert(argument->name).second) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "duplicate generic argument '" +
                                                                      argument->name + "'");
                }
                valid = false;
                continue;
            }

            ordered[parameter_index] = argument;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (ordered[i] != nullptr) {
                continue;
            }

            if (emit_errors) {
                context.diagnostics.add_error(span, "missing generic argument '" +
                                                        parameters[i].name + "'");
            }
            valid = false;
        }

        if (!valid) {
            return false;
        }

        arguments = std::move(ordered);
        return true;
    }

    bool apply_call_generic_bindings(std::vector<GenericArgument*>& arguments,
                                     const std::vector<GenericParameterType>& parameters, Span span,
                                     bool emit_errors) {
        if (!reorder_generic_arguments(arguments, parameters, span, emit_errors)) {
            return false;
        }

        GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
        return arguments_resolver.apply(arguments, parameters, emit_errors);
    }

    std::vector<GenericParameterType>
    get_call_generic_parameters(const ResolvedCallable& candidate) const {
        const auto* function_type =
            candidate.symbol != nullptr ? candidate.symbol->function_type : nullptr;
        if (function_type == nullptr) {
            return {};
        }

        if (candidate.has_receiver() && !candidate.parent.empty()) {
            std::size_t parent_parameter_count = 0;
            if (const auto* parent_symbol = sema.env.current_scope->lookup_type(candidate.parent);
                parent_symbol != nullptr) {
                if (const auto* declared_nominal = parent_symbol->type->as_nominal();
                    declared_nominal != nullptr) {
                    parent_parameter_count = declared_nominal->generic_parameters.size();
                }
            }

            const auto* receiver_type = candidate.receiver->type;
            if (const auto* pointer = receiver_type->as<PointerType>(); pointer != nullptr) {
                receiver_type = pointer->element;
            }

            if (parent_parameter_count == 0) {
                if (const auto* nominal = receiver_type->as_nominal(); nominal != nullptr) {
                    parent_parameter_count = nominal->generic_parameters.size();
                }
            }

            if (parent_parameter_count <= function_type->generic_parameters.size()) {
                return std::vector<GenericParameterType>(
                    function_type->generic_parameters.begin() +
                        static_cast<std::ptrdiff_t>(parent_parameter_count),
                    function_type->generic_parameters.end());
            }
        }

        return function_type->generic_parameters;
    }

    std::vector<ResolvedCallable> collect_direct_candidates(const std::string& name, Span span) {
        std::vector<ResolvedCallable> candidates;

        const auto& overloads = sema.env.current_scope->lookup_function_overloads(name);
        candidates.reserve(overloads.size());

        for (auto* symbol : overloads) {
            if (symbol != nullptr && symbol->function_type != nullptr) {
                candidates.emplace_back(symbol);
            }
        }

        if (!current_parent.empty()) {
            const auto* parent_symbol = sema.env.current_scope->lookup_type(current_parent);
            if (parent_symbol != nullptr && parent_symbol->type != nullptr) {
                auto* type_scope = member_resolver.resolve_nominal_scope(parent_symbol->type);
                if (type_scope != nullptr) {
                    const auto* overloads = type_scope->find_local_function_overloads(name);
                    if (overloads != nullptr) {
                        for (auto* symbol : *overloads) {
                            if (symbol != nullptr && symbol->function_type != nullptr &&
                                MemberResolver::is_visible(symbol->visibility, current_parent,
                                                           current_parent)) {
                                if (symbol->callable_kind ==
                                    FunctionSymbol::Kind::Type::InstanceMethod) {
                                    if (sema.env.current_scope->lookup_var("self") != nullptr) {
                                        auto* self_ident =
                                            sema.nodes.create<IdentifierExpression>(span, "self");
                                        visitor.visit_expression(*self_ident);
                                        candidates.emplace_back(symbol, self_ident, current_parent);
                                    }
                                } else {
                                    candidates.emplace_back(symbol, nullptr, current_parent);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (candidates.empty()) {
            const auto* type_symbol = sema.env.current_scope->lookup_type(name);
            if (type_symbol != nullptr && type_symbol->type != nullptr &&
                type_symbol->type->is<StructType>()) {
                auto* type_scope = member_resolver.resolve_nominal_scope(type_symbol->type);
                if (type_scope != nullptr) {
                    const auto* overloads = type_scope->find_local_function_overloads(name);
                    if (overloads != nullptr) {
                        for (auto* symbol : *overloads) {
                            if (symbol != nullptr && symbol->function_type != nullptr &&
                                MemberResolver::is_visible(symbol->visibility, current_parent,
                                                           name)) {
                                candidates.emplace_back(symbol, nullptr, name);
                            }
                        }
                    }
                }
            }
        }

        return candidates;
    }

    std::optional<std::vector<Argument*>>
    normalize_arguments(const std::vector<Argument*>& arguments,
                        const std::vector<ParameterType>& parameters, Span span, bool is_variadic,
                        bool emit_errors) {
        if (arguments.size() < parameters.size() && !is_variadic) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "too few arguments: expected " + std::to_string(parameters.size()) +
                              ", got " + std::to_string(arguments.size()));
            }
            return std::nullopt;
        }

        if (arguments.size() > parameters.size() && !is_variadic) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "too many arguments: expected " + std::to_string(parameters.size()) +
                              ", got " + std::to_string(arguments.size()));
            }
            return std::nullopt;
        }

        std::vector<Argument*> normalized(parameters.size(), nullptr);
        std::unordered_set<std::string> used_names;
        used_names.reserve(arguments.size());

        bool seen_named = false;
        bool valid = true;
        std::size_t positional_index = 0;

        for (auto* argument : arguments) {
            if (argument->name.empty()) {
                if (seen_named) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            argument->span, "positional argument cannot follow named argument");
                    }
                    valid = false;
                    continue;
                }

                if (positional_index < parameters.size()) {
                    normalized[positional_index] = argument;
                    used_names.insert(parameters[positional_index].name);
                    ++positional_index;
                } else if (is_variadic) {
                    normalized.push_back(argument);
                }
                continue;
            }

            seen_named = true;

            std::size_t parameter_index = 0;
            for (; parameter_index < parameters.size(); ++parameter_index) {
                if (parameters[parameter_index].name == argument->name) {
                    break;
                }
            }

            if (parameter_index == parameters.size()) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span,
                                                  "unknown parameter '" + argument->name + "'");
                }
                valid = false;
                continue;
            }

            if (!used_names.insert(argument->name).second) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span,
                                                  "duplicate argument '" + argument->name + "'");
                }
                valid = false;
                continue;
            }

            normalized[parameter_index] = argument;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (normalized[i] != nullptr) {
                continue;
            }

            if (emit_errors) {
                context.diagnostics.add_error(span,
                                              "missing argument '" + parameters[i].name + "'");
            }
            valid = false;
        }

        if (!valid) {
            return std::nullopt;
        }

        return normalized;
    }

    void apply_candidate_parent_bindings(const ResolvedCallable& candidate) {
        if (candidate.parent.empty()) {
            return;
        }

        if (candidate.has_receiver() && candidate.receiver->type != nullptr) {
            const auto* receiver_type = candidate.receiver->type;
            if (const auto* ptr = receiver_type->as<PointerType>()) {
                receiver_type = ptr->element;
            }
            member_resolver.apply_parent_substitutions(candidate.parent, receiver_type);
            return;
        }

        const auto* parent_symbol = sema.env.current_scope->lookup_type(candidate.parent);
        if (parent_symbol != nullptr) {
            resolver.bind_type_parameter("Self", parent_symbol->type);
        }
    }

    int score_candidate(const ResolvedCallable& candidate, const std::vector<Argument*>& arguments,
                        std::vector<GenericArgument*>& generic_arguments, Span span) {
        if (candidate.symbol == nullptr || candidate.symbol->function_type == nullptr) {
            return -1;
        }

        const auto* func_type = candidate.symbol->function_type;
        auto effective_parameters = func_type->parameters;

        if (candidate.has_receiver() && !effective_parameters.empty() &&
            effective_parameters.front().name == "self") {
            effective_parameters.erase(effective_parameters.begin());
        }

        auto normalized_args =
            normalize_arguments(arguments, effective_parameters, span, func_type->variadic, false);
        if (!normalized_args.has_value()) {
            return -1;
        }

        auto scope = resolver.create_substitution_scope();

        apply_candidate_parent_bindings(candidate);

        auto call_generic_parameters = get_call_generic_parameters(candidate);

        if (!call_generic_parameters.empty()) {
            auto call_generics = generic_arguments;
            if (call_generics.empty()) {
                call_generics = infer_from_arguments(call_generic_parameters, effective_parameters,
                                                     *normalized_args, span);
            }

            if (call_generics.size() == call_generic_parameters.size()) {
                if (!apply_call_generic_bindings(call_generics, call_generic_parameters, span,
                                                 false)) {
                    return -1;
                }
            } else if (!call_generics.empty()) {
                return -1;
            }
        }

        int score = 0;
        if (candidate.has_receiver()) {
            score += 10;
        }

        for (std::size_t i = 0; i < effective_parameters.size(); ++i) {
            auto* arg = (*normalized_args)[i];
            if (arg == nullptr || arg->value == nullptr || arg->value->type == nullptr) {
                continue;
            }

            const auto* param_type = resolver.resolve_type(effective_parameters[i].type);
            const auto* arg_type = arg->value->type;

            if (param_type == nullptr || arg_type == nullptr) {
                return -1;
            }

            if (param_type->accepts(arg_type)) {
                score += 2;
            } else if (param_type->is<IntegerType>() && arg_type->is<IntegerType>()) {
                score += 1;
            } else if (param_type->is<FloatType>() && arg_type->is<FloatType>()) {
                score += 1;
            } else if (facades.accepts(param_type, arg_type)) {
                score += 1;
            } else if (TypeCoercion::classify(resolver, param_type, arg_type) !=
                       Coercion::Type::None) {
                score += 1;
            } else {
                return -1;
            }
        }

        return score;
    }

    void typecheck_and_coerce_arguments(CallExpression& node, const FunctionType& function_type,
                                        bool has_implicit_receiver) {
        auto effective_parameters = function_type.parameters;
        if (has_implicit_receiver && !effective_parameters.empty() &&
            effective_parameters.front().name == "self") {
            effective_parameters.erase(effective_parameters.begin());
        }

        auto normalized = normalize_arguments(node.arguments, effective_parameters, node.span,
                                              function_type.variadic, true);
        if (!normalized.has_value()) {
            return;
        }

        node.arguments = std::move(*normalized);

        for (std::size_t i = 0; i < node.arguments.size(); ++i) {
            auto* argument = node.arguments[i];
            if (argument == nullptr || argument->value == nullptr) {
                continue;
            }

            const Type* expected_type = nullptr;
            if (i < effective_parameters.size()) {
                expected_type = resolver.resolve_type(effective_parameters[i].type);
            }

            const auto* saved_target = target_type;
            if (expected_type != nullptr) {
                target_type = expected_type;
            }
            visitor.visit_expression(*argument->value);
            target_type = saved_target;

            const auto* actual_type = argument->value->type;
            if (expected_type != nullptr && actual_type != nullptr) {
                if (!facades.accepts(expected_type, actual_type)) {
                    context.diagnostics.add_error(argument->span,
                                                  "type mismatch in call argument: expected '" +
                                                      expected_type->to_string() + "', got '" +
                                                      actual_type->to_string() + "'");
                }

                argument->value =
                    TypeCoercion::apply(sema, resolver, argument->value, expected_type);
            }
        }
    }

    static bool is_receiver_argument(const Argument& argument, Expression* receiver) {
        if (argument.value == receiver) {
            return true;
        }

        auto* unary = argument.value->as<UnaryExpression>();
        if (unary == nullptr || unary->operand != receiver) {
            return false;
        }

        return unary->op == UnaryExpression::Operator::Type::AddressOf ||
               unary->op == UnaryExpression::Operator::Type::AddressOfMut;
    }

    std::vector<Argument*> extract_explicit_arguments(CallExpression& node) const {
        auto arguments = node.arguments;
        auto* member = node.callee->as<MemberExpression>();
        if (member == nullptr || arguments.empty()) {
            return arguments;
        }

        if (is_receiver_argument(*arguments.front(), member->value)) {
            arguments.erase(arguments.begin());
        }

        return arguments;
    }

    std::vector<Argument*> build_call_arguments(const std::vector<Argument*>& explicit_arguments,
                                                Expression* receiver,
                                                const FunctionType* function_type) {
        std::vector<Argument*> arguments;
        arguments.reserve(explicit_arguments.size() + (receiver != nullptr ? 1 : 0));

        if (receiver != nullptr) {
            auto* value = receiver;

            if (function_type != nullptr && !function_type->parameters.empty()) {
                const auto* self_type =
                    resolver.resolve_type(function_type->parameters.front().type);

                if (const auto* pointer_type =
                        self_type != nullptr ? self_type->as<PointerType>() : nullptr) {
                    if (receiver->type == nullptr || !receiver->type->is<PointerType>()) {
                        const auto op = pointer_type->is_mutable
                                            ? UnaryExpression::Operator::Type::AddressOfMut
                                            : UnaryExpression::Operator::Type::AddressOf;

                        value = sema.nodes.create<UnaryExpression>(receiver->span, op, receiver);
                        value->type = self_type;
                    }
                }
            }

            arguments.push_back(sema.nodes.create<Argument>(receiver->span, "", value));
        }

        arguments.insert(arguments.end(), explicit_arguments.begin(), explicit_arguments.end());
        return arguments;
    }

  public:
    explicit CallResolver(Context& context, SemaContext& sema, TypeResolver& resolver,
                          Visitor<void>& visitor, const std::string& current_parent,
                          const Type*& target_type)
        : context(context), sema(sema), resolver(resolver), facades(sema, resolver),
          member_resolver(context, sema, resolver, facades, current_parent), visitor(visitor),
          current_parent(current_parent), target_type(target_type) {}

    void resolve(CallExpression& node) {
        node.arguments = extract_explicit_arguments(node);

        for (auto* argument : node.arguments) {
            if (argument != nullptr && argument->value != nullptr) {
                visitor.visit_expression(*argument->value);
            }
        }

        std::vector<ResolvedCallable> candidates;

        if (auto* ident = node.callee->as<IdentifierExpression>()) {
            const auto* var_symbol = sema.env.current_scope->lookup_var(ident->name);
            if (var_symbol != nullptr && var_symbol->type != nullptr &&
                var_symbol->type->is<FunctionType>()) {
                visitor.visit_expression(*node.callee);
                const auto* func_type = var_symbol->type->as<FunctionType>();
                typecheck_and_coerce_arguments(node, *func_type, false);
                node.resolved_target = std::make_unique<IndirectCallTarget>(*func_type);
                node.type = resolver.resolve_type(func_type->return_type);
                return;
            }

            candidates = collect_direct_candidates(ident->name, node.span);
        } else if (auto* member_expr = node.callee->as<MemberExpression>()) {
            visitor.visit_expression(*member_expr->value);
            candidates = member_resolver.resolve_callables(*member_expr);
        } else if (auto* qualified = node.callee->as<QualifiedAccessExpression>()) {
            candidates = member_resolver.resolve_callables(*qualified);
        } else {
            visitor.visit_expression(*node.callee);
            const auto* identifier = node.callee->as<IdentifierExpression>();
            const auto named_function =
                identifier != nullptr &&
                !sema.env.current_scope->lookup_function_overloads(identifier->name).empty();
            if (!named_function && node.callee->type != nullptr &&
                node.callee->type->is<FunctionType>()) {
                const auto* func_type = node.callee->type->as<FunctionType>();
                typecheck_and_coerce_arguments(node, *func_type, false);
                node.type = resolver.resolve_type(func_type->return_type);
                return;
            }
        }

        if (candidates.empty()) {
            context.diagnostics.add_error(node.span, "cannot resolve call target");
            return;
        }

        ResolvedCallable best_candidate(nullptr);
        int best_score = -1;
        bool ambiguous = false;

        for (const auto& candidate : candidates) {
            int score =
                score_candidate(candidate, node.arguments, node.generic_arguments, node.span);
            if (score > best_score) {
                best_score = score;
                best_candidate = candidate;
                ambiguous = false;
            } else if (score == best_score && score >= 0) {
                ambiguous = true;
            }
        }

        if (best_score < 0 || best_candidate.symbol == nullptr) {
            context.diagnostics.add_error(node.span, "no matching overload for call");
            return;
        }

        if (ambiguous) {
            context.diagnostics.add_error(node.span, "ambiguous call to overloaded function");
            return;
        }

        if (best_candidate.has_receiver()) {
            if (auto* ident = node.callee->as<IdentifierExpression>()) {
                auto* member_expr = sema.nodes.create<MemberExpression>(
                    node.span, best_candidate.receiver, ident->name);
                member_expr->type = best_candidate.symbol->function_type;
                node.callee = member_expr;
            }
        }

        node.resolved_target = std::make_unique<DirectCallTarget>(*best_candidate.symbol);
        auto* func_type = best_candidate.symbol->function_type;

        auto scope = resolver.create_substitution_scope();

        apply_candidate_parent_bindings(best_candidate);

        auto call_generic_parameters = get_call_generic_parameters(best_candidate);

        if (!call_generic_parameters.empty()) {
            if (node.generic_arguments.empty()) {
                auto effective_params = func_type->parameters;
                if (best_candidate.has_receiver() && !effective_params.empty() &&
                    effective_params.front().name == "self") {
                    effective_params.erase(effective_params.begin());
                }

                auto inferred = infer_from_arguments(call_generic_parameters, effective_params,
                                                     node.arguments, node.span);
                if (!inferred.empty()) {
                    node.generic_arguments = std::move(inferred);
                }
            }

            apply_call_generic_bindings(node.generic_arguments, call_generic_parameters, node.span,
                                        true);
        }

        if (best_candidate.has_receiver()) {
            node.arguments =
                build_call_arguments(node.arguments, best_candidate.receiver, func_type);
        }

        typecheck_and_coerce_arguments(node, *func_type, false);

        const auto* resolved_return = resolver.resolve_type(func_type->return_type);
        node.type = resolved_return != nullptr ? resolved_return
                                               : sema.builtin_resolver.primitives.at("void");
    }
};
