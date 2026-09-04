module;

#include <string>
#include <vector>

export module zep.frontend.sema.declaration.function;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.type.resolver;

export class FunctionDeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    TypeBuilder& builder;
    FacadeResolver& facades;

    const FunctionType* resolve_with_self_type(const FunctionType* type, const Type* self_type) {
        auto scope = resolver.create_substitution_scope();
        const auto* nominal = self_type != nullptr ? self_type->as_nominal() : nullptr;
        if (nominal != nullptr) {
            for (const auto& parameter : nominal->generic_parameters) {
                resolver.bind_generic_parameter(parameter, true);
            }
        }

        resolver.bind_type_parameter("Self", self_type);

        const auto* resolved = resolver.resolve_type(type);
        return resolved != nullptr ? resolved->as<FunctionType>() : nullptr;
    }

    bool matches_extension_owner(const Type* receiver_type, const Type* owner_type) {
        if (facades.accepts(receiver_type, owner_type)) {
            return true;
        }

        const auto* receiver_nominal =
            receiver_type != nullptr ? receiver_type->as_nominal() : nullptr;
        const auto* owner_nominal = owner_type != nullptr ? owner_type->as_nominal() : nullptr;

        return receiver_nominal != nullptr && owner_nominal != nullptr &&
               receiver_nominal->same_nominal(*owner_nominal);
    }

  public:
    FunctionDeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                               TypeBuilder& builder, FacadeResolver& facades)
        : context(context), sema(sema), resolver(resolver), builder(builder), facades(facades) {}

    bool method_signature_matches(const FunctionType* expected, const FunctionType* actual,
                                  const Type* self_type) {
        const auto* resolved_expected = resolve_with_self_type(expected, self_type);
        const auto* resolved_actual = resolve_with_self_type(actual, self_type);

        if (resolved_expected == nullptr || resolved_actual == nullptr) {
            return false;
        }

        if (resolved_expected->return_type == nullptr || resolved_actual->return_type == nullptr) {
            return false;
        }

        if (!resolved_expected->return_type->accepts(resolved_actual->return_type)) {
            return false;
        }

        if (resolved_expected->parameters.size() != resolved_actual->parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < resolved_expected->parameters.size(); ++i) {
            const auto* expected_param = resolved_expected->parameters[i].type;
            const auto* actual_param = resolved_actual->parameters[i].type;

            if (expected_param == nullptr || actual_param == nullptr) {
                return false;
            }

            const auto* expected_pointer = expected_param->as<PointerType>();
            const auto* actual_pointer = actual_param->as<PointerType>();

            if (expected_pointer != nullptr && actual_pointer != nullptr && i == 0) {
                const auto* expected_element = resolver.resolve_type(expected_pointer->element);
                const auto* actual_element = resolver.resolve_type(actual_pointer->element);
                const auto* expected_struct =
                    expected_element != nullptr ? expected_element->as<StructType>() : nullptr;
                const auto* actual_struct =
                    actual_element != nullptr ? actual_element->as<StructType>() : nullptr;

                if (expected_struct != nullptr && actual_struct != nullptr &&
                    (expected_struct->inherits_from(actual_struct) ||
                     actual_struct->inherits_from(expected_struct) ||
                     expected_struct->same_nominal(*actual_struct))) {
                    continue;
                }
            }

            if (!expected_param->accepts(actual_param)) {
                return false;
            }
        }

        return true;
    }

    bool validate_function_declaration(FunctionDeclaration& node, const FunctionSymbol* symbol) {
        if (symbol == nullptr || symbol->function_type == nullptr) {
            return false;
        }

        const auto* owner_symbol =
            node.is_member() ? sema.env.current_scope->lookup_type(node.parent) : nullptr;
        const auto* owner_type = owner_symbol != nullptr ? owner_symbol->type : nullptr;
        const auto* owner_struct_type =
            owner_type != nullptr ? owner_type->as<StructType>() : nullptr;

        const auto* return_type = resolver.resolve_type(symbol->function_type->return_type);

        if (node.is_extension) {
            const auto expected_kind = node.is_static ? FunctionSymbol::Kind::Type::StaticMethod
                                                      : FunctionSymbol::Kind::Type::InstanceMethod;
            if (node.is_override || node.kind() != expected_kind) {
                context.diagnostics.add_error(node.span, "invalid extension method declaration");
                return false;
            }
        }

        for (auto* parameter : node.prototype->parameters) {
            if (parameter->name == "self") {
                context.diagnostics.add_error(parameter->span, "methods use implicit self");
                return false;
            }
        }

        if (node.is_static &&
            node.prototype->receiver_kind != FunctionPrototype::ReceiverKind::Type::None) {
            context.diagnostics.add_error(node.span, "static method cannot be declared 'mut'");
            return false;
        }

        switch (node.kind()) {
        case FunctionSymbol::Kind::Type::Function:
            return true;

        case FunctionSymbol::Kind::Type::InstanceMethod: {
            if (node.prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::None) {
                context.diagnostics.add_error(node.span, "instance method '" +
                                                             node.prototype->name +
                                                             "' must declare an implicit receiver");
                return false;
            }

            if (node.is_extension && owner_type != nullptr) {
                const auto* self_type =
                    resolver.resolve_type(symbol->function_type->parameters.front().type);
                const auto* receiver_type = self_type;

                if (const auto* pointer_type =
                        self_type != nullptr ? self_type->as<PointerType>() : nullptr) {
                    receiver_type = resolver.resolve_type(pointer_type->element);
                }

                if (receiver_type == nullptr ||
                    !matches_extension_owner(receiver_type, owner_type)) {
                    context.diagnostics.add_error(
                        node.span, "extension method receiver must match '" + node.parent + "'");
                    return false;
                }
            }

            return true;
        }

        case FunctionSymbol::Kind::Type::StaticMethod: {
            return true;
        }

        case FunctionSymbol::Kind::Type::Constructor: {
            if (node.is_static) {
                context.diagnostics.add_error(node.span, "constructor cannot be declared static");
                return false;
            }

            if (owner_struct_type == nullptr || return_type == nullptr ||
                !facades.accepts(return_type, owner_struct_type)) {
                context.diagnostics.add_error(node.span, "constructor '" + node.prototype->name +
                                                             "' must return '" + node.parent + "'");
                return false;
            }

            return true;
        }

        case FunctionSymbol::Kind::Type::Destructor: {
            if (node.is_static) {
                context.diagnostics.add_error(node.span, "destructor cannot be declared static");
                return false;
            }

            if (return_type == nullptr || !return_type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "destructor '" + node.prototype->name +
                                                             "' must return void");
                return false;
            }

            if (!node.prototype->parameters.empty()) {
                context.diagnostics.add_error(node.span, "destructor '" + node.prototype->name +
                                                             "' must not declare parameters");
                return false;
            }

            const auto* self_type =
                resolver.resolve_type(symbol->function_type->parameters.front().type);
            const auto* pointer_type =
                self_type != nullptr ? self_type->as<PointerType>() : nullptr;
            const auto* element_type =
                pointer_type != nullptr ? pointer_type->element->as<StructType>() : nullptr;

            if (pointer_type == nullptr || !pointer_type->is_mutable || element_type == nullptr ||
                element_type->name != node.parent) {
                context.diagnostics.add_error(node.span, "destructor receiver must be '*mut " +
                                                             node.parent + "'");
                return false;
            }

            const auto* owner = sema.env.current_scope->lookup_type(node.parent);
            auto* member_scope = owner != nullptr ? owner->member_scope : nullptr;
            if (member_scope != nullptr) {
                const auto* overloads =
                    member_scope->find_local_function_overloads(node.prototype->name);
                if (overloads != nullptr && overloads->size() > 1) {
                    context.diagnostics.add_error(node.span, "duplicate destructor for struct '" +
                                                                 node.parent + "'");
                    return false;
                }
            }

            return true;
        }
        }

        return true;
    }

    const FunctionSymbol* declare_function(FunctionDeclaration& node) {
        if (node.function_symbol != nullptr) {
            return node.function_symbol;
        }

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(
            attribute_infos,
            {sema, context, node.span, node.prototype->name, node.prototype->generic_parameters});

        if (node.type == nullptr) {
            std::vector<GenericParameterType> parent_generics;
            const Type* self_type = nullptr;
            if (node.is_member() || node.is_extension) {
                const auto* parent_symbol = sema.env.current_scope->lookup_type(node.parent);
                self_type = parent_symbol != nullptr ? parent_symbol->type : nullptr;
                const auto* nominal = self_type != nullptr ? self_type->as_nominal() : nullptr;
                if (nominal != nullptr && node.prototype->generic_parameters.empty()) {
                    parent_generics = nominal->generic_parameters;
                }
            }

            node.type = builder.build_function_type(*node.prototype, parent_generics, self_type);
        }

        const auto* type = node.type->as<FunctionType>();
        if (type == nullptr) {
            return nullptr;
        }

        auto* target_scope = sema.env.current_scope;
        if (node.is_member()) {
            const auto* owner = sema.env.current_scope->lookup_type(node.parent);
            if (owner != nullptr && owner->member_scope != nullptr) {
                target_scope = owner->member_scope;
            }
        }

        const auto* existing_overloads =
            target_scope->find_local_function_overloads(node.prototype->name);
        if (existing_overloads != nullptr) {
            for (auto* symbol : *existing_overloads) {
                const auto* function_type = symbol->type->as<FunctionType>();
                if (function_type == nullptr) {
                    continue;
                }

                if (function_type->conflicts_with(*type)) {
                    context.diagnostics.add_error(node.span, "redefinition of function '" +
                                                                 node.prototype->name + "'");
                    return nullptr;
                }
            }
        }

        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, Linkage::Type::Internal, type,
            node.kind(), node.parent, std::move(attribute_infos), node.is_extension,
            Abi::Type::Language);

        target_scope->define_function(node.prototype->name, symbol,
                                      sema.env.overloads.create<OverloadSet>());

        node.function_symbol = symbol;
        sema.function_definitions.add(*symbol, node);

        return symbol;
    }

    void declare_extern_function(ExternFunctionDeclaration& node) {
        if (node.function_symbol != nullptr) {
            return;
        }

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(
            attribute_infos,
            {sema, context, node.span, node.prototype->name, node.prototype->generic_parameters});

        if (node.type != nullptr) {
            for (auto* symbol :
                 sema.env.current_scope->lookup_function_overloads(node.prototype->name)) {
                if (symbol->type == node.type) {
                    return;
                }
            }
        }

        const auto* type = builder.build_function_type(*node.prototype);
        node.type = type;

        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, Linkage::Type::External, type,
            FunctionSymbol::Kind::Type::Function, std::string{}, std::move(attribute_infos), false,
            Abi::Type::C);
        sema.env.current_scope->define_function(node.prototype->name, symbol,
                                                sema.env.overloads.create<OverloadSet>());
        node.function_symbol = symbol;
    }
};
