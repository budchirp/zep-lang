module;

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

export module zep.frontend.sema.declaration;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.frontend.kind;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
export import zep.frontend.sema.declaration.function;
import zep.frontend.sema.declaration.attribute;
export import zep.frontend.sema.declaration.nominal;

export class DeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    DeclarationTypeBuilder& builder;
    Visitor<void>& visitor;

  public:
    Facades facades;
    Compatibility compatibility;
    FunctionDeclarationChecker functions;
    NominalDeclarationChecker nominals;

    DeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                       DeclarationTypeBuilder& builder, Visitor<void>& visitor,
                       std::function<void(Expression&, const Type*)> check_expression)
        : context(context), sema(sema), resolver(resolver), builder(builder), visitor(visitor),
          facades(sema.facades, resolver), compatibility(sema.types, resolver, facades),
          functions(context, sema, resolver, builder, compatibility),
          nominals(context, sema, resolver, builder, facades, functions, visitor,
                   std::move(check_expression)) {
    }

    void declare_type_alias(TypeAliasDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        for (auto* generic_parameter : node.generic_parameters) {
            visitor.visit(*generic_parameter);
        }

        auto generic_parameter_types =
            builder.build_generic_parameter_types(node.generic_parameters);

        auto substitution_scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameters(generic_parameter_types);

        visitor.visit(*node.target);

        const auto* target_type = resolver.resolve_type(node.target->type);
        node.type = target_type;

        auto* symbol =
            sema.env.symbols.create<TypeSymbol>(node.name, node.span, node.visibility, target_type);

        if (!sema.env.current_scope->define_type(node.name, symbol)) {
            context.diagnostics.add_error(node.span, "redefinition of type '" + node.name + "'");
        }
    }

    template <typename Declaration>
    void declare_variable(Declaration& node) {
        if (node.type != nullptr) {
            return;
        }

        if (node.annotation != nullptr) {
            visitor.visit(*node.annotation);
        }

        node.type = node.annotation != nullptr ? resolver.resolve_type(node.annotation->type) : nullptr;

        auto storage = StorageKind::Type::Var;

        if constexpr (std::is_same_v<Declaration, VarDeclaration>) {
            storage = node.storage_kind;
        }

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(attribute_infos, {sema, context, node.span, node.name, {}});

        auto* symbol = sema.env.symbols.create<VariableSymbol>(
            node.name, node.span, node.visibility, storage, node.type,
            std::move(attribute_infos));
        node.variable_symbol = symbol;

        if (!sema.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }

    void declare_program_symbols(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* interface_declaration = statement->as<InterfaceDeclaration>();
                interface_declaration != nullptr) {
                nominals.declare_interface_type(*interface_declaration);
            }

            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                nominals.declare_struct_type(*struct_declaration);
            }

            if (auto* enum_declaration = statement->as<EnumDeclaration>();
                enum_declaration != nullptr) {
                nominals.declare_enum_type(*enum_declaration);
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                functions.declare_function(*function_declaration);
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                functions.declare_extern_function(*extern_function_declaration);
            }

            if (auto* var_declaration = statement->as<VarDeclaration>();
                var_declaration != nullptr) {
                declare_variable(*var_declaration);
            }

            if (auto* extern_var_declaration = statement->as<ExternVarDeclaration>();
                extern_var_declaration != nullptr) {
                declare_variable(*extern_var_declaration);
            }

            if (auto* type_alias = statement->as<TypeAliasDeclaration>(); type_alias != nullptr) {
                declare_type_alias(*type_alias);
            }
        }
    }
};
