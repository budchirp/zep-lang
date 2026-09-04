module;

#include <functional>
#include <string>
#include <vector>

export module zep.frontend.sema.declaration;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.type.resolver;
export import zep.frontend.sema.declaration.function;
export import zep.frontend.sema.declaration.variable;
export import zep.frontend.sema.declaration.struct_checker;
export import zep.frontend.sema.declaration.enum_checker;

export class DeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    TypeBuilder& builder;
    Visitor<void>& visitor;

  public:
    FacadeResolver facades;
    FunctionDeclarationChecker functions;
    VariableDeclarationChecker variables;
    StructDeclarationChecker structs;
    EnumDeclarationChecker enums;

    const FunctionSymbol* current_function = nullptr;
    std::string current_parent;

    DeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                       TypeBuilder& builder, Visitor<void>& visitor,
                       std::function<void(Expression&, const Type*)> check_expression)
        : context(context), sema(sema), resolver(resolver), builder(builder), visitor(visitor),
          facades(sema, resolver), functions(context, sema, resolver, builder, facades),
          variables(context, sema, resolver, visitor),
          structs(context, sema, builder, facades, functions,
                  [this](EnumDeclaration& node) { enums.declare_enum_type(node); }),
          enums(context, sema, resolver, builder, functions, visitor, std::move(check_expression)) {
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
        resolver.bind_generic_parameters(generic_parameter_types, true);

        visitor.visit(*node.target);

        const auto* target_type = resolver.resolve_type(node.target->type);
        node.type = target_type;

        auto* symbol =
            sema.env.symbols.create<TypeSymbol>(node.name, node.span, node.visibility, target_type);

        if (!sema.env.current_scope->define_type(node.name, symbol)) {
            context.diagnostics.add_error(node.span, "redefinition of type '" + node.name + "'");
        }
    }

    void declare_interface_type(InterfaceDeclaration& node) {
        structs.declare_interface_type(node);
    }

    void declare_struct_type(StructDeclaration& node) { structs.declare_struct_type(node); }

    void declare_enum_type(EnumDeclaration& node) { enums.declare_enum_type(node); }

    const FunctionSymbol* declare_function(FunctionDeclaration& node) {
        return functions.declare_function(node);
    }

    void declare_extern_function(ExternFunctionDeclaration& node) {
        functions.declare_extern_function(node);
    }

    void declare_variable(VarDeclaration& node) { variables.declare_variable(node); }

    void declare_extern_variable(ExternVarDeclaration& node) {
        variables.declare_extern_variable(node);
    }

    bool validate_function_declaration(FunctionDeclaration& node, const FunctionSymbol* symbol) {
        return functions.validate_function_declaration(node, symbol);
    }

    void declare_program_symbols(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* interface_declaration = statement->as<InterfaceDeclaration>();
                interface_declaration != nullptr) {
                declare_interface_type(*interface_declaration);
            }

            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                declare_struct_type(*struct_declaration);
            }

            if (auto* enum_declaration = statement->as<EnumDeclaration>();
                enum_declaration != nullptr) {
                declare_enum_type(*enum_declaration);
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                declare_function(*function_declaration);
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                declare_extern_function(*extern_function_declaration);
            }

            if (auto* var_declaration = statement->as<VarDeclaration>();
                var_declaration != nullptr) {
                declare_variable(*var_declaration);
            }

            if (auto* extern_var_declaration = statement->as<ExternVarDeclaration>();
                extern_var_declaration != nullptr) {
                declare_extern_variable(*extern_var_declaration);
            }

            if (auto* type_alias = statement->as<TypeAliasDeclaration>(); type_alias != nullptr) {
                declare_type_alias(*type_alias);
            }
        }
    }
};
