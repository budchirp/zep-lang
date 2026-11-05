module;

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.resolution.callable;

import zep.common.context;
import zep.common.source.span;
import zep.common.state;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.scopes.environment;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.kind;
import zep.frontend.sema.expression.values;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.expression.generic;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.sema.expression.coercion;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.resolution.arguments;
import zep.frontend.sema.generics.environment;

export class ResolvedCallable {
  public:
    FunctionSymbol* symbol;
    Expression* receiver;
    std::string parent;

    explicit ResolvedCallable(FunctionSymbol* symbol, Expression* receiver = nullptr,
                              std::string parent = {})
        : symbol(symbol), receiver(receiver), parent(std::move(parent)) {}

};

export class MemberResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    const Facades& facades;
    const NominalType* enclosing_type;

    void resolve_scope_member(MemberExpression& node) {
        auto* scope = node.value->scope;

        if (const auto* type_symbol = scope->find_exported_type(node.member);
            type_symbol != nullptr) {
            node.type = type_symbol->type;
            node.scope = Scope::members(type_symbol->type);
            return;
        }

        auto function_overloads = scope->find_exported_function_overloads(node.member);
        if (!function_overloads.empty()) {
            node.type = function_overloads.front()->type;
            return;
        }

        context.diagnostics.add_error(node.span, "scope '" + scope->name + "' has no member '" +
                                                     node.member + "'");
    }

    void resolve_facade_member(MemberExpression& node, const Type* value_type) {
        auto* facade_scope = facades.scope(value_type);
        if (facade_scope == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot access member of non-struct/enum type '" +
                                              node.value->type->to_string() + "'");
            return;
        }

        if (node.member == "value") {
            node.type = facades.value(value_type);
            return;
        }

        if (facade_scope->has_local_function(node.member)) {
            context.diagnostics.add_error(node.span, "instance method '" + node.member +
                                                         "' must be called with '()'");
            return;
        }

        context.diagnostics.add_error(node.span, "cannot access member of non-struct/enum type '" +
                                                     node.value->type->to_string() + "'");
    }

    void resolve_enum_member(MemberExpression& node, const EnumType* enum_type) {
        if (node.member == "value") {
            node.type = enum_type->backing_type != nullptr
                            ? resolver.resolve_type(enum_type->backing_type)
                            : sema.primitives.entries.at("i32");
            const auto* qualified = node.value->as<QualifiedAccessExpression>();
            if (qualified != nullptr) {
                node.compile_time_value = qualified->enum_value;
            }
            return;
        }
    }

    void resolve_struct_field(MemberExpression& node, const NominalType* nominal,
                              const Type* value_type) {
        auto* type_scope = resolve_nominal_scope(value_type);
        if (type_scope == nullptr) {
            context.diagnostics.add_error(node.span, nominal->label + " '" + nominal->name +
                                                         "' has no field '" + node.member + "'");
            return;
        }

        if (const auto* field = type_scope->find_local_var(node.member); field != nullptr) {
            if (!nominal->is<StructType>()) {
                context.diagnostics.add_error(
                    node.span, "enum '" + nominal->name + "' has no field '" + node.member + "'");
                return;
            }

            const auto* field_type = nominal->as<StructType>()->find_field(node.member);
            if (!is_visible(field->visibility, enclosing_type, nominal)) {
                context.diagnostics.add_error(node.span, "cannot access private field '" +
                                                             field->name + "' of struct '" +
                                                             nominal->name + "'");
            }

            node.type =
                resolver.resolve_type(field_type != nullptr ? field_type->type : field->type);
            return;
        }

        if (type_scope->has_local_function(node.member)) {
            context.diagnostics.add_error(node.span, "instance method '" + node.member +
                                                         "' must be called with '()'");
            return;
        }

        context.diagnostics.add_error(node.span, nominal->label + " '" + nominal->name +
                                                     "' has no field '" + node.member + "'");
    }

    std::vector<GenericArgumentType>
    make_argument_types(const std::vector<GenericArgument*>& generic_arguments) const {
        std::vector<GenericArgumentType> argument_types;
        argument_types.reserve(generic_arguments.size());

        for (auto* argument : generic_arguments) {
            if (argument->const_binding.has_value()) {
                argument_types.emplace_back(argument->name, *argument->const_binding);
            } else {
                argument_types.emplace_back(
                    argument->name, argument->type != nullptr ? argument->type->type : nullptr);
            }
        }

        return argument_types;
    }

  public:
    explicit MemberResolver(Context& context, SemaContext& sema, TypeResolver& resolver,
                            const Facades& facades, const NominalType* enclosing_type)
        : context(context), sema(sema), resolver(resolver), facades(facades),
          enclosing_type(enclosing_type) {}

    void apply_parent_substitutions(const std::string& parent, const Type* actual_parent_type) {
        if (parent.empty() || actual_parent_type == nullptr) {
            return;
        }

        if (actual_parent_type->as_nominal() == nullptr) {
            if (const auto* wrapper = facades.wrapper(actual_parent_type); wrapper != nullptr) {
                actual_parent_type = wrapper;
            }
        }

        const auto* declared_nominal = actual_parent_type->as_nominal();
        if (const auto* parent_symbol = sema.env.current_scope->lookup_type(parent);
            parent_symbol != nullptr) {
            declared_nominal = parent_symbol->type->as_nominal();
        } else if (declared_nominal != nullptr && declared_nominal->definition != nullptr) {
            declared_nominal = declared_nominal->definition;
        }

        const auto* actual_nominal = actual_parent_type->as_nominal();
        if (declared_nominal == nullptr || actual_nominal == nullptr) {
            return;
        }

        resolver.bind_type_parameter("Self", actual_parent_type);

        const auto count = std::min(declared_nominal->generic_parameters.size(),
                                    actual_nominal->generic_arguments.size());

        for (std::size_t index = 0; index < count; ++index) {
            const auto& parameter = declared_nominal->generic_parameters[index];
            resolver.bind_generic_binding(parameter.name,
                                          actual_nominal->generic_arguments[index].binding(),
                                          parameter.declaration);
        }
    }

    void attach_static_method(QualifiedAccessExpression& node,
                              const std::vector<FunctionSymbol*>& candidates,
                              const Type* parent_type) {
        if (!is_visible(candidates.front()->visibility, enclosing_type,
                        parent_type->as_nominal())) {
            context.diagnostics.add_error(
                node.span, "cannot access private static method '" + node.member + "' of " +
                               parent_type->label + " '" + node.parent + "'");
            return;
        }

        if (candidates.size() > 1) {
            context.diagnostics.add_warning(node.span, "ambiguous reference to static method '" +
                                                           node.member + "'");
        }

        auto scope = resolver.create_substitution_scope();
        apply_parent_substitutions(node.parent, parent_type);

        node.function_symbol = candidates.front();
        node.type = resolver.resolve_type(node.function_symbol->type);
    }

    bool resolve_static_method(QualifiedAccessExpression& node, const Type* parent_type,
                               const Type* declared_type) {
        auto* type_scope = resolve_nominal_scope(declared_type);
        if (type_scope == nullptr) {
            return false;
        }

        const auto* overloads = type_scope->find_local_function_overloads(node.member);
        if (overloads == nullptr || overloads->empty()) {
            return false;
        }

        attach_static_method(node, *overloads, parent_type);
        return true;
    }

    void resolve_qualified_enum_member(QualifiedAccessExpression& node,
                                       const TypeSymbol* type_symbol) {
        const auto* parent_type =
            resolve_nominal_type(node.parent, node.parent_generic_arguments, node.span);
        const auto* enum_type = parent_type != nullptr ? parent_type->as<EnumType>() : nullptr;
        if (enum_type == nullptr) {
            return;
        }

        if (const auto* variant = enum_type->find_variant(node.member); variant != nullptr) {
            if (!variant->fields.empty()) {
                context.diagnostics.add_error(node.span, "enum variant '" + node.member +
                                                             "' requires payload fields");
                return;
            }

            node.enum_type = enum_type;
            node.variant_type = variant;
            if (!variant->discriminant.has_value()) {
                context.diagnostics.add_error(node.span, "enum discriminant is not resolved for '" +
                                                             node.member + "'");
                return;
            }
            node.enum_value = variant->discriminant;
            node.type = enum_type;

            return;
        }

        if (resolve_static_method(node, enum_type, type_symbol->type)) {
            return;
        }

        context.diagnostics.add_error(node.span, "enum '" + enum_type->name +
                                                     "' has no variant or member '" + node.member +
                                                     "'");
    }

    void resolve_qualified_static_member(QualifiedAccessExpression& node,
                                         const TypeSymbol* type_symbol) {
        const auto* parent_type =
            resolve_nominal_type(node.parent, node.parent_generic_arguments, node.span);
        if (parent_type == nullptr || !parent_type->is<StructType>()) {
            return;
        }

        if (resolve_static_method(node, parent_type, type_symbol->type)) {
            return;
        }

        context.diagnostics.add_error(node.span, "type '" + node.parent + "' has no member '" +
                                                     node.member + "'");
    }

    static bool is_visible(Visibility::Type visibility, const NominalType* enclosing_type,
                           const NominalType* owner) {
        const auto* enclosing_identity = enclosing_type != nullptr &&
                                                 enclosing_type->definition != nullptr
                                             ? enclosing_type->definition
                                             : enclosing_type;
        const auto* owner_identity = owner != nullptr && owner->definition != nullptr
                                         ? owner->definition
                                         : owner;

        return visibility == Visibility::Type::Public ||
               (enclosing_identity != nullptr && enclosing_identity == owner_identity);
    }

    Scope* resolve_nominal_scope(const Type* type) const {
        const auto* resolved = resolver.resolve_type(type);
        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto* ptr = resolved->as<PointerType>()) {
            resolved = resolver.resolve_type(ptr->element);
            if (resolved == nullptr) {
                return nullptr;
            }
        }

        const auto* nominal = resolved->as_nominal();
        if (nominal == nullptr) {
            return nullptr;
        }

        return Scope::members(nominal);
    }

    const Type* resolve_nominal_type(const std::string& parent,
                                     const std::vector<GenericArgument*>& generic_arguments,
                                     Span span) {
        const auto* type_symbol = sema.env.current_scope->lookup_type(parent);
        if (type_symbol == nullptr) {
            context.diagnostics.add_error(span, "use of undeclared type '" + parent + "'");
            return nullptr;
        }

        const auto* nominal = type_symbol->type->as_nominal();
        if (nominal == nullptr) {
            context.diagnostics.add_error(span, "expected different type for '" + parent +
                                                    "', got " + type_symbol->type->label + " type");
            return nullptr;
        }

        if (generic_arguments.empty()) {
            return type_symbol->type;
        }

        const auto* named =
            sema.types.create<NamedType>(parent, make_argument_types(generic_arguments));
        return resolver.resolve_type(named);
    }

    std::vector<ResolvedCallable> resolve_callables(MemberExpression& node) {
        std::vector<ResolvedCallable> callables;
        auto* receiver = node.value;

        if (receiver->scope != nullptr) {
            const auto* overloads = receiver->scope->find_local_function_overloads(node.member);
            if (overloads != nullptr) {
                callables.reserve(overloads->size());
                for (auto* symbol : *overloads) {
                    if (symbol != nullptr && symbol->visibility == Visibility::Type::Public) {
                        callables.emplace_back(symbol);
                    }
                }
            }

            if (!callables.empty()) {
                return callables;
            }

            const auto* type_symbol = receiver->scope->find_exported_type(node.member);
            auto* type_scope =
                type_symbol != nullptr ? resolve_nominal_scope(type_symbol->type) : nullptr;
            const auto* constructors = type_scope != nullptr
                                           ? type_scope->find_local_function_overloads(node.member)
                                           : nullptr;
            if (constructors == nullptr) {
                return callables;
            }

            callables.reserve(constructors->size());
            for (auto* symbol : *constructors) {
                if (symbol != nullptr && symbol->visibility == Visibility::Type::Public) {
                    callables.emplace_back(symbol, nullptr, node.member);
                }
            }

            return callables;
        }

        const auto* receiver_type = receiver->type;
        if (receiver_type == nullptr) {
            return callables;
        }

        const auto* effective_receiver = receiver_type;
        if (const auto* pointer = receiver_type->as<PointerType>(); pointer != nullptr) {
            effective_receiver = resolver.resolve_type(pointer->element);
        }

        const auto* variable = receiver->as<IdentifierExpression>();
        const auto receiver_is_type =
            variable != nullptr && sema.env.current_scope->lookup_var(variable->name) == nullptr &&
            sema.env.current_scope->lookup_type(variable->name) != nullptr;

        const auto* nominal =
            effective_receiver != nullptr ? effective_receiver->as_nominal() : nullptr;
        if (nominal != nullptr) {
            auto* scope = resolve_nominal_scope(effective_receiver);
            auto* overloads =
                scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;

            while (overloads == nullptr) {
                const auto* structure = nominal->as<StructType>();

                if (structure == nullptr || structure->base_type == nullptr) {
                    break;
                }

                const auto* base = resolver.resolve_type(structure->base_type);
                nominal = base != nullptr ? base->as_nominal() : nullptr;

                if (nominal == nullptr) {
                    break;
                }

                scope = resolve_nominal_scope(nominal);
                overloads = scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;
            }

            if (overloads == nullptr) {
                return callables;
            }

            callables.reserve(overloads->size());
            for (auto* symbol : *overloads) {
                if (symbol == nullptr ||
                    (!symbol->extension() &&
                     !is_visible(symbol->visibility, enclosing_type, nominal))) {
                    continue;
                }

                if (receiver_is_type) {
                    if (symbol->static_method() || symbol->constructor()) {
                        callables.emplace_back(symbol, nullptr, nominal->name);
                    }
                    continue;
                }

                if (symbol->instance()) {
                    callables.emplace_back(symbol, receiver, nominal->name);
                }
            }

            return callables;
        }

        auto* scope = facades.scope(receiver_type);
        const auto* overloads =
            scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;
        if (overloads == nullptr) {
            return callables;
        }

        const auto* facade = facades.wrapper(receiver_type);
        callables.reserve(overloads->size());
        for (auto* symbol : *overloads) {
            if (symbol != nullptr && symbol->visibility == Visibility::Type::Public) {
                callables.emplace_back(symbol, receiver,
                                       facade != nullptr ? facade->name : std::string());
            }
        }

        return callables;
    }

    std::vector<ResolvedCallable> resolve_callables(QualifiedAccessExpression& node) {
        std::vector<ResolvedCallable> callables;
        const auto* parent_type =
            resolve_nominal_type(node.parent, node.parent_generic_arguments, node.span);
        if (parent_type == nullptr) {
            return callables;
        }

        auto* scope = resolve_nominal_scope(parent_type);
        const auto* overloads =
            scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;
        if (overloads == nullptr) {
            return callables;
        }

        callables.reserve(overloads->size());
        for (auto* symbol : *overloads) {
            if (symbol != nullptr &&
                (symbol->extension() ||
                 is_visible(symbol->visibility, enclosing_type,
                            parent_type->as_nominal()))) {
                callables.emplace_back(symbol, nullptr, node.parent);
            }
        }

        return callables;
    }

    void resolve_member(MemberExpression& node) {
        if (node.value->scope != nullptr) {
            resolve_scope_member(node);
            return;
        }

        const auto* value_type = node.value->type;
        if (value_type == nullptr) {
            return;
        }

        const auto* nominal = value_type->as_nominal();
        if (nominal == nullptr) {
            resolve_facade_member(node, value_type);
            return;
        }

        if (const auto* enum_type = value_type->as<EnumType>(); enum_type != nullptr) {
            resolve_enum_member(node, enum_type);
            if (node.type != nullptr) {
                return;
            }
        }

        resolve_struct_field(node, nominal, value_type);
    }

    void resolve_qualified(QualifiedAccessExpression& node) {
        auto qualified_name = node.parent + "::" + node.member;

        const auto* member_symbol = sema.env.current_scope->lookup_type(qualified_name);
        if (member_symbol != nullptr) {
            node.type = member_symbol->type;
            return;
        }

        auto* type_symbol = sema.env.current_scope->lookup_type(node.parent);
        if (type_symbol == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "use of undeclared type '" + node.parent + "'");
            return;
        }

        switch (type_symbol->type->kind) {
        case Type::Kind::Type::Enum:
            resolve_qualified_enum_member(node, type_symbol);
            return;
        case Type::Kind::Type::Struct:
            resolve_qualified_static_member(node, type_symbol);
            return;
        default:
            context.diagnostics.add_error(node.span, "cannot access member '" + node.member +
                                                         "' of type '" +
                                                         type_symbol->type->to_string() + "'");
            return;
        }
    }
};

export class CallResolver {
  private:
    class Match {
      public:
        ResolvedCallable candidate;
        int score;
        std::vector<Argument*> arguments;
        std::vector<GenericArgument*> generics;
        const FunctionType* signature;
        CompileTimeEnvironment bindings;

        Match(ResolvedCallable candidate, int score, std::vector<Argument*> arguments,
              std::vector<GenericArgument*> generics, const FunctionType* signature,
              CompileTimeEnvironment bindings)
            : candidate(std::move(candidate)), score(score), arguments(std::move(arguments)),
              generics(std::move(generics)), signature(signature), bindings(std::move(bindings)) {}
    };

    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    Facades facades;
    Compatibility compatibility;
    MemberResolver member_resolver;
    Visitor<void>& visitor;
    const NominalType* enclosing_type;
    const Type*& target_type;

    std::vector<GenericParameterType>
    get_call_generic_parameters(const ResolvedCallable& candidate) const {
        const auto* function_type =
            candidate.symbol != nullptr ? candidate.symbol->signature() : nullptr;
        if (function_type == nullptr) {
            return {};
        }

        if (candidate.receiver != nullptr && !candidate.parent.empty()) {
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
                    const auto* definition = nominal->definition != nullptr
                                                 ? nominal->definition
                                                 : nominal;
                    parent_parameter_count = definition->generic_parameters.size();
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
            if (symbol != nullptr && symbol->signature() != nullptr) {
                candidates.emplace_back(symbol);
            }
        }

        if (enclosing_type != nullptr) {
            auto* type_scope = member_resolver.resolve_nominal_scope(enclosing_type);
            if (type_scope != nullptr) {
                const auto* overloads = type_scope->find_local_function_overloads(name);
                if (overloads != nullptr) {
                    for (auto* symbol : *overloads) {
                        if (symbol == nullptr || symbol->signature() == nullptr ||
                            !MemberResolver::is_visible(symbol->visibility, enclosing_type,
                                                        enclosing_type)) {
                            continue;
                        }

                        if (symbol->instance()) {
                            if (sema.env.current_scope->lookup_var("self") != nullptr) {
                                auto* self_ident =
                                    sema.nodes.create<IdentifierExpression>(span, "self");
                                visitor.visit_expression(*self_ident);
                                candidates.emplace_back(symbol, self_ident,
                                                        enclosing_type->name);
                            }
                        } else {
                            candidates.emplace_back(symbol, nullptr, enclosing_type->name);
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
                            if (symbol != nullptr && symbol->signature() != nullptr &&
                                MemberResolver::is_visible(
                                    symbol->visibility, enclosing_type,
                                    type_symbol->type->as_nominal())) {
                                candidates.emplace_back(symbol, nullptr, name);
                            }
                        }
                    }
                }
            }
        }

        return candidates;
    }

    void apply_candidate_parent_bindings(const ResolvedCallable& candidate) {
        if (candidate.parent.empty()) {
            return;
        }

        if (candidate.receiver != nullptr && candidate.receiver->type != nullptr) {
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

    std::optional<Match> match_candidate(const ResolvedCallable& candidate, const std::vector<Argument*>& arguments,
                        std::vector<GenericArgument*>& generic_arguments, Span span) {
        if (candidate.symbol == nullptr || candidate.symbol->signature() == nullptr) {
            return std::nullopt;
        }

        const auto* function_type = candidate.symbol->signature();
        std::span<const ParameterType> effective_parameters(function_type->parameters);

        if (candidate.receiver != nullptr && !effective_parameters.empty() &&
            effective_parameters.front().name == "self") {
            effective_parameters = effective_parameters.subspan(1);
        }

        auto normalized =
            Arguments::normalize(context, arguments, effective_parameters, span, function_type->variadic, false);
        if (!normalized.has_value()) {
            return std::nullopt;
        }

        auto scope = resolver.create_substitution_scope();

        apply_candidate_parent_bindings(candidate);

        auto call_generic_parameters = get_call_generic_parameters(candidate);
        auto call_generics = generic_arguments;

        if (!call_generic_parameters.empty()) {
            if (call_generics.empty()) {
                call_generics = Arguments::infer(context, sema, resolver, call_generic_parameters, effective_parameters,
                                                     *normalized, span);
            }

            if (call_generics.size() == call_generic_parameters.size()) {
                GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
                if (!arguments_resolver.apply(call_generics, call_generic_parameters, span, false)) {
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        }

        int score = 0;
        if (candidate.receiver != nullptr) {
            score += 10;
            const auto moving = Values::moving(*candidate.receiver);
            const auto temporary = !Values::place(*candidate.receiver);
            if (candidate.symbol->has_attribute(".move_receiver")) {
                if (!moving && !temporary) {
                    return std::nullopt;
                }
                score += 3;
            }
        }

        for (std::size_t i = 0; i < effective_parameters.size(); ++i) {
            auto* argument = (*normalized)[i];
            if (argument == nullptr || argument->value == nullptr || argument->value->type == nullptr) {
                continue;
            }

            const auto* parameter_type = resolver.resolve_type(effective_parameters[i].type);
            const auto* argument_type = argument->value->type;

            if (parameter_type == nullptr || argument_type == nullptr) {
                return std::nullopt;
            }

            const auto* reference = parameter_type->as<PointerType>();
            const auto moving = Values::moving(*argument->value);
            if (reference != nullptr && reference->reference_kind == PointerType::ReferenceKind::Type::Move) {
                if (!moving && Values::place(*argument->value)) {
                    return std::nullopt;
                }
                score += 2;
            }

            if (parameter_type->accepts(argument_type)) {
                score += 2;
            } else if (parameter_type->is<IntegerType>() && argument_type->is<IntegerType>()) {
                score += 1;
            } else if (parameter_type->is<FloatType>() && argument_type->is<FloatType>()) {
                score += 1;
            } else if (compatibility.accepts(parameter_type, argument_type)) {
                score += 1;
            } else if (TypeCoercion::classify(resolver, parameter_type, argument_type) !=
                       Coercion::Type::None) {
                score += 1;
            } else {
                return std::nullopt;
            }
        }

        return Match(candidate, score, std::move(*normalized), std::move(call_generics),
                     resolver.resolve_type(function_type)->as<FunctionType>(), resolver.substitutions);
    }

    void bind_arguments(CallExpression& node, const FunctionType& function_type) {
        for (std::size_t i = 0; i < node.arguments.size(); ++i) {
            auto* argument = node.arguments[i];
            if (argument == nullptr || argument->value == nullptr) {
                continue;
            }

            const Type* expected_type = nullptr;
            if (i < function_type.parameters.size()) {
                expected_type = resolver.resolve_type(function_type.parameters[i].type);
            }

            const auto* saved_target = target_type;
            if (expected_type != nullptr) {
                target_type = expected_type;
            }
            auto* receiver_borrow = i == 0 && !function_type.parameters.empty() && function_type.parameters[0].name == "self"
                ? argument->value->as<UnaryExpression>() : nullptr;
            const auto temporary_receiver = receiver_borrow != nullptr && !Values::place(*receiver_borrow->operand);
            if (temporary_receiver && receiver_borrow->op == UnaryOperator::Type::AddressOfMut) {
                visitor.visit_expression(*receiver_borrow->operand);
                receiver_borrow->type = expected_type;
            } else {
                visitor.visit_expression(*argument->value);
            }
            target_type = saved_target;

            const auto* actual_type = argument->value->type;
            if (expected_type != nullptr && actual_type != nullptr) {
                if (!compatibility.accepts(expected_type, actual_type)) {
                    context.diagnostics.add_error(argument->span,
                                                  "type mismatch in call argument: expected '" +
                                                      expected_type->to_string() + "', got '" +
                                                      actual_type->to_string() + "'");
                }

                sema.binding.reference(*argument->value, expected_type, sema.env, context.diagnostics);
                argument->value =
                    TypeCoercion::apply(sema, resolver, argument->value, expected_type);
                argument->value =
                    sema.binding.construct(argument->value, expected_type, resolver, context.diagnostics);
            }
        }
    }

    void resolve_indirect(CallExpression& node, const FunctionType& signature) {
        auto normalized = Arguments::normalize(context, node.arguments, signature.parameters,
                                               node.span, signature.variadic, true);

        if (!normalized.has_value()) {
            return;
        }

        node.arguments = std::move(*normalized);
        bind_arguments(node, signature);
        const auto* resolved = resolver.resolve_type(&signature)->as<FunctionType>();
        node.resolved_target = std::make_unique<IndirectCallTarget>(*resolved);
        node.type = resolved->return_type;
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
                          Visitor<void>& visitor, const NominalType* enclosing_type,
                          const Type*& target_type)
        : context(context), sema(sema), resolver(resolver),
          facades(sema.facades, resolver), compatibility(sema.types, resolver, facades),
          member_resolver(context, sema, resolver, facades, enclosing_type), visitor(visitor),
          enclosing_type(enclosing_type), target_type(target_type) {}

    void resolve(CallExpression& node) {
        node.arguments = extract_explicit_arguments(node);

        std::vector<ResolvedCallable> candidates;

        if (auto* ident = node.callee->as<IdentifierExpression>()) {
            const auto* var_symbol = sema.env.current_scope->lookup_var(ident->name);
            if (var_symbol != nullptr && var_symbol->type != nullptr &&
                var_symbol->type->is<FunctionType>()) {
                visitor.visit_expression(*node.callee);
                resolve_indirect(node, *var_symbol->type->as<FunctionType>());
                return;
            }

            candidates = collect_direct_candidates(ident->name, node.span);
        } else if (auto* member = node.callee->as<MemberExpression>(); member != nullptr) {
            visitor.visit_expression(*member->value);
            candidates = member_resolver.resolve_callables(*member);
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
                resolve_indirect(node, *node.callee->type->as<FunctionType>());
                return;
            }
        }

        if (candidates.empty()) {
            context.diagnostics.add_error(node.span, "cannot resolve call target");
            return;
        }

        std::span<const ParameterType> contextual_parameters;
        std::optional<std::vector<Argument*>> contextual_arguments;
        if (candidates.size() == 1 && get_call_generic_parameters(candidates.front()).empty()) {
            const auto& candidate = candidates.front();
            contextual_parameters = candidate.symbol->signature()->parameters;
            if (candidate.receiver != nullptr && !contextual_parameters.empty() &&
                contextual_parameters.front().name == "self") {
                contextual_parameters = contextual_parameters.subspan(1);
            }
            contextual_arguments = Arguments::normalize(context,
                node.arguments, contextual_parameters, node.span,
                candidate.symbol->signature()->variadic, false);
        }

        for (auto* argument : node.arguments) {
            if (argument == nullptr || argument->value == nullptr) {
                continue;
            }

            const auto* saved_target = target_type;
            target_type = nullptr;
            if (contextual_arguments.has_value()) {
                const auto found = std::ranges::find(*contextual_arguments, argument);
                const auto index = static_cast<std::size_t>(found - contextual_arguments->begin());
                if (index < contextual_parameters.size()) {
                    const auto* expected = resolver.resolve_type(contextual_parameters[index].type);
                    if (resolver.is_concrete(expected)) {
                        target_type = expected;
                    }
                }
            }
            visitor.visit_expression(*argument->value);
            target_type = saved_target;
        }

        std::optional<Match> best;
        auto ambiguous = false;

        for (const auto& candidate : candidates) {
            auto match = match_candidate(candidate, node.arguments, node.generic_arguments, node.span);

            if (!match.has_value()) {
                continue;
            }

            if (!best.has_value() || match->score > best->score) {
                best = std::move(match);
                ambiguous = false;
            } else if (match->score == best->score) {
                ambiguous = true;
            }
        }

        if (!best.has_value()) {
            if (candidates.size() == 1 && node.generic_arguments.empty()) {
                const auto& candidate = candidates.front();
                auto scope = resolver.create_substitution_scope();
                apply_candidate_parent_bindings(candidate);
                const auto parameters = get_call_generic_parameters(candidate);
                std::span<const ParameterType> effective_parameters(
                    candidate.symbol->signature()->parameters);
                if (candidate.receiver != nullptr && !effective_parameters.empty() &&
                    effective_parameters.front().name == "self") {
                    effective_parameters = effective_parameters.subspan(1);
                }
                auto normalized = Arguments::normalize(context,
                    node.arguments, effective_parameters, node.span,
                    candidate.symbol->signature()->variadic, false);
                if (!parameters.empty() && normalized.has_value() &&
                    Arguments::infer(context, sema, resolver, parameters, effective_parameters, *normalized,
                                         node.span, true).empty()) {
                    return;
                }
            }

            context.diagnostics.add_error(node.span, "no matching overload for call");
            return;
        }

        if (ambiguous) {
            context.diagnostics.add_error(node.span, "ambiguous call to overloaded function");
            return;
        }

        const auto& best_candidate = best->candidate;

        if (sema.lifecycle.deleted(best_candidate.symbol)) {
            context.diagnostics.add_error(node.span, "use of deleted function '" + best_candidate.symbol->name + "'");
            return;
        }

        if (best_candidate.receiver != nullptr) {
            if (auto* ident = node.callee->as<IdentifierExpression>()) {
                auto* member = sema.nodes.create<MemberExpression>(
                    node.span, best_candidate.receiver, ident->name);
                member->type = best_candidate.symbol->type;
                node.callee = member;
            }
        }

        node.resolved_target = std::make_unique<DirectCallTarget>(*best_candidate.symbol, best->signature);
        const auto* function_type = best->signature;
        node.arguments = std::move(best->arguments);
        node.generic_arguments = std::move(best->generics);

        StateGuard substitution_guard(resolver.substitutions, std::move(best->bindings));

        if (best_candidate.receiver != nullptr) {
            node.arguments =
                build_call_arguments(node.arguments, best_candidate.receiver, function_type);
        }

        bind_arguments(node, *function_type);

        const auto* resolved_return = resolver.resolve_type(function_type->return_type);
        node.type = resolved_return != nullptr ? resolved_return
                                               : sema.primitives.entries.at("void");
    }
};
