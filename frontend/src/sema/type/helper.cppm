module;

#include <string>

export module zep.frontend.sema.type.helper;

import zep.frontend.sema.context;
import zep.frontend.ast;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.scope;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.kind;
import zep.frontend.sema.env;
import zep.frontend.ast.program;

export class TypeHelper {
  private:
    Visitor<void>& visitor;

    Context& context;

    TypeResolver& resolver;
    TypeBuilder& builder;

  public:
    const FunctionSymbol* current_function = nullptr;

    explicit TypeHelper(Visitor<void>& visitor, Context& context, TypeResolver& resolver,
                        TypeBuilder& builder)
        : visitor(visitor), context(context), resolver(resolver), builder(builder) {}

    void register_declarations(Program& program) {
        for (auto* statement : program.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                define_struct(*struct_declaration);
                continue;
            }

            if (auto* var_declaration = statement->as<VarDeclaration>();
                var_declaration != nullptr) {
                define_variable(*var_declaration);
                continue;
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                define_function(*function_declaration);
                continue;
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                define_extern_function(*extern_function_declaration);
                continue;
            }

            if (auto* extern_var_declaration = statement->as<ExternVarDeclaration>();
                extern_var_declaration != nullptr) {
                define_extern_variable(*extern_var_declaration);
            }
        }
    }

    void define_struct(StructDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        const auto* type = builder.build_struct(node);
        node.type = type;

        auto* symbol =
            context.symbols.create<TypeSymbol>(node.name, node.span, node.visibility, type);

        if (!context.env.current_scope->define_type(node.name, symbol)) {
            context.diagnostics.add_error(node.span, "redefinition of type '" + node.name + "'");
        }
    }

    FunctionSymbol* define_function(FunctionDeclaration& node) {
        if (node.type != nullptr) {
            for (auto* symbol :
                 context.env.current_scope->lookup_function_overloads(node.prototype->name)) {
                if (symbol->type == node.type) {
                    return symbol;
                }
            }

            return nullptr;
        }

        const auto* type = builder.build_function(*node.prototype);
        node.type = type;

        for (auto* symbol :
             context.env.current_scope->lookup_function_overloads(node.prototype->name)) {
            const auto* function_type = symbol->type->as<FunctionType>();
            if (function_type == nullptr) {
                continue;
            }

            if (function_type->conflicts_with(*type)) {
                context.diagnostics.add_warning(node.span, "duplicate signature for function '" +
                                                               node.prototype->name + "'");
                break;
            }
        }

        auto* symbol = context.symbols.create<FunctionSymbol>(node.prototype->name, node.span,
                                                              node.visibility, type);

        context.env.current_scope->define_function(node.prototype->name, symbol);

        return symbol;
    }

    void define_extern_function(ExternFunctionDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        const auto* type = builder.build_function(*node.prototype);
        node.type = type;

        auto* symbol = context.symbols.create<FunctionSymbol>(node.prototype->name, node.span,
                                                              node.visibility, type);
        context.env.current_scope->define_function(node.prototype->name, symbol);
    }

    void define_variable(VarDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        if (node.annotation != nullptr) {
            visitor.visit(*node.annotation);
        }

        const Type* type = nullptr;
        if (node.annotation != nullptr) {
            type = resolver.resolve_type(node.annotation->type);
        }
        node.type = type;

        auto* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                         node.storage_kind, type);

        if (!context.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }

    void define_extern_variable(ExternVarDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        visitor.visit(*node.annotation);

        const auto* type = resolver.resolve_type(node.annotation->type);
        node.type = type;

        auto* symbol = context.symbols.create<VarSymbol>(node.name, node.span, node.visibility,
                                                         StorageKind::Type::Var, type);

        if (!context.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }
};
