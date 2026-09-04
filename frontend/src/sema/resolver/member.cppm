module;

#include <algorithm>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.resolver.member;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class ResolvedCallable {
  public:
    FunctionSymbol* symbol;
    Expression* receiver;
    std::string parent;

    explicit ResolvedCallable(FunctionSymbol* symbol, Expression* receiver = nullptr,
                              std::string parent = {})
        : symbol(symbol), receiver(receiver), parent(std::move(parent)) {}

    bool has_receiver() const { return receiver != nullptr; }
};

export class MemberResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    const FacadeResolver& facades;
    const std::string& current_parent;

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
                            : sema.builtin_resolver.primitives.at("i32");
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
            if (!is_visible(field->visibility, current_parent, nominal->name)) {
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
                            const FacadeResolver& facades, const std::string& current_parent)
        : context(context), sema(sema), resolver(resolver), facades(facades),
          current_parent(current_parent) {}

    void apply_parent_substitutions(const std::string& parent, const Type* actual_parent_type) {
        if (parent.empty() || actual_parent_type == nullptr) {
            return;
        }

        const auto* declared_nominal = actual_parent_type->as_nominal();
        if (const auto* parent_symbol = sema.env.current_scope->lookup_type(parent);
            parent_symbol != nullptr) {
            declared_nominal = parent_symbol->type->as_nominal();
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
        if (!is_visible(candidates.front()->visibility, current_parent, node.parent)) {
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

    static bool is_visible(Visibility::Type visibility, const std::string& current_parent,
                           const std::string& symbol_parent) {
        return visibility == Visibility::Type::Public || current_parent == symbol_parent;
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
                     !is_visible(symbol->visibility, current_parent, nominal->name))) {
                    continue;
                }

                const auto kind = symbol->callable_kind;
                if (receiver_is_type) {
                    if (kind == FunctionSymbol::Kind::Type::StaticMethod ||
                        kind == FunctionSymbol::Kind::Type::Constructor) {
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
                 is_visible(symbol->visibility, current_parent, node.parent))) {
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
