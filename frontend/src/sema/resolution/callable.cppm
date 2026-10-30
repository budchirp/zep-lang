module;

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.resolution.callable;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.kind;
import zep.frontend.sema.lifetime;
import zep.frontend.sema.facade;
import zep.frontend.sema.expression.generic;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.expression.coercion;
import zep.frontend.sema.type_resolver;

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
    const FacadeResolver& facades;
    const NominalType* enclosing_type;

    void resolve_scope_member(MemberExpression& node) {
        auto* scope = node.value->scope;

        if (const auto* type_symbol = scope->find_exported_type(node.member);
            type_symbol != nullptr) {
            node.type = type_symbol->type;
            node.scope = type_symbol->member_scope;
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
        auto* facade_scope = facades.resolve_scope(value_type);
        if (facade_scope == nullptr) {
            context.diagnostics.add_error(node.span,
                                          "cannot access member of non-struct/enum type '" +
                                              node.value->type->to_string() + "'");
            return;
        }

        if (node.member == "value") {
            node.type = facades.resolve_value(value_type);
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
                            const FacadeResolver& facades, const NominalType* enclosing_type)
        : context(context), sema(sema), resolver(resolver), facades(facades),
          enclosing_type(enclosing_type) {}

    void apply_parent_substitutions(const std::string& parent, const Type* actual_parent_type) {
        if (parent.empty() || actual_parent_type == nullptr) {
            return;
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
        node.type = resolver.resolve_type(node.function_symbol->function_type);
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

        if (nominal->member_scope != nullptr) {
            return static_cast<Scope*>(nominal->member_scope);
        }

        if (nominal->definition != nullptr && nominal->definition != nominal &&
            nominal->definition->member_scope != nullptr) {
            return static_cast<Scope*>(nominal->definition->member_scope);
        }

        if (const auto* symbol = sema.env.current_scope->lookup_type(nominal->name);
            symbol != nullptr && symbol->member_scope != nullptr) {
            return symbol->member_scope;
        }

        if (sema.env.root_scope != nullptr) {
            if (const auto* root_symbol = sema.env.root_scope->lookup_type(nominal->name);
                root_symbol != nullptr && root_symbol->member_scope != nullptr) {
                return root_symbol->member_scope;
            }
        }

        return nullptr;
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
            const auto* overloads =
                scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;
            if (overloads == nullptr) {
                return callables;
            }

            callables.reserve(overloads->size());
            for (auto* symbol : *overloads) {
                if (symbol == nullptr ||
                    (!symbol->is_extension &&
                     !is_visible(symbol->visibility, enclosing_type, nominal))) {
                    continue;
                }

                const auto kind = symbol->callable_kind;
                if (receiver_is_type) {
                    if (kind == FunctionSymbol::Kind::Type::StaticMethod ||
                        kind == FunctionSymbol::Kind::Type::Constructor ||
                        kind == FunctionSymbol::Kind::Type::CopyConstructor) {
                        callables.emplace_back(symbol, nullptr, nominal->name);
                    }
                    continue;
                }

                if (kind == FunctionSymbol::Kind::Type::InstanceMethod) {
                    callables.emplace_back(symbol, receiver, nominal->name);
                }
            }

            return callables;
        }

        auto* scope = facades.resolve_scope(receiver_type);
        const auto* overloads =
            scope != nullptr ? scope->find_local_function_overloads(node.member) : nullptr;
        if (overloads == nullptr) {
            return callables;
        }

        const auto* facade = facades.resolve_facade(receiver_type);
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
                (symbol->is_extension ||
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
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    FacadeResolver facades;
    MemberResolver member_resolver;
    Visitor<void>& visitor;
    const NominalType* enclosing_type;
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
                         std::span<const ParameterType> declared_parameters,
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

    std::vector<GenericParameterType>
    get_call_generic_parameters(const ResolvedCallable& candidate) const {
        const auto* function_type =
            candidate.symbol != nullptr ? candidate.symbol->function_type : nullptr;
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
            if (symbol != nullptr && symbol->function_type != nullptr) {
                candidates.emplace_back(symbol);
            }
        }

        if (enclosing_type != nullptr) {
            auto* type_scope = member_resolver.resolve_nominal_scope(enclosing_type);
            if (type_scope != nullptr) {
                const auto* overloads = type_scope->find_local_function_overloads(name);
                if (overloads != nullptr) {
                    for (auto* symbol : *overloads) {
                        if (symbol == nullptr || symbol->function_type == nullptr ||
                            !MemberResolver::is_visible(symbol->visibility, enclosing_type,
                                                        enclosing_type)) {
                            continue;
                        }

                        if (symbol->callable_kind ==
                            FunctionSymbol::Kind::Type::InstanceMethod) {
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
                            if (symbol != nullptr && symbol->function_type != nullptr &&
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

    std::optional<std::vector<Argument*>>
    normalize_arguments(const std::vector<Argument*>& arguments,
                        std::span<const ParameterType> parameters, Span span, bool is_variadic,
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

    int score_candidate(const ResolvedCallable& candidate, const std::vector<Argument*>& arguments,
                        std::vector<GenericArgument*>& generic_arguments, Span span) {
        if (candidate.symbol == nullptr || candidate.symbol->function_type == nullptr) {
            return -1;
        }

        const auto* func_type = candidate.symbol->function_type;
        std::span<const ParameterType> effective_parameters(func_type->parameters);

        if (candidate.receiver != nullptr && !effective_parameters.empty() &&
            effective_parameters.front().name == "self") {
            effective_parameters = effective_parameters.subspan(1);
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
                GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
                if (!arguments_resolver.apply(call_generics, call_generic_parameters, span, false)) {
                    return -1;
                }
            } else if (!call_generics.empty()) {
                return -1;
            }
        }

        int score = 0;
        if (candidate.receiver != nullptr) {
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

    void typecheck_and_coerce_arguments(CallExpression& node, const FunctionType& function_type) {
        auto normalized = normalize_arguments(node.arguments,
                                              std::span<const ParameterType>(function_type.parameters),
                                              node.span, function_type.variadic, true);
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
            if (i < function_type.parameters.size()) {
                expected_type = resolver.resolve_type(function_type.parameters[i].type);
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
                argument->value =
                    sema.lifetime.copy_construct(argument->value, expected_type, resolver);
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
                          Visitor<void>& visitor, const NominalType* enclosing_type,
                          const Type*& target_type)
        : context(context), sema(sema), resolver(resolver),
          facades(sema.facades, sema.types, resolver),
          member_resolver(context, sema, resolver, facades, enclosing_type), visitor(visitor),
          enclosing_type(enclosing_type), target_type(target_type) {}

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
                typecheck_and_coerce_arguments(node, *func_type);
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
                typecheck_and_coerce_arguments(node, *func_type);
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

        if (best_candidate.receiver != nullptr) {
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
                std::span<const ParameterType> effective_parameters(func_type->parameters);
                if (best_candidate.receiver != nullptr && !effective_parameters.empty() &&
                    effective_parameters.front().name == "self") {
                    effective_parameters = effective_parameters.subspan(1);
                }

                auto inferred = infer_from_arguments(call_generic_parameters, effective_parameters,
                                                     node.arguments, node.span);
                if (!inferred.empty()) {
                    node.generic_arguments = std::move(inferred);
                }
            }

            GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
            arguments_resolver.apply(node.generic_arguments, call_generic_parameters, node.span,
                                     true);
        }

        if (best_candidate.receiver != nullptr) {
            node.arguments =
                build_call_arguments(node.arguments, best_candidate.receiver, func_type);
        }

        typecheck_and_coerce_arguments(node, *func_type);

        const auto* resolved_return = resolver.resolve_type(func_type->return_type);
        node.type = resolved_return != nullptr ? resolved_return
                                               : sema.primitives.entries.at("void");
    }
};
