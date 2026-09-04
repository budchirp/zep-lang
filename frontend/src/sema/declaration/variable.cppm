module;

#include <string>
#include <vector>

export module zep.frontend.sema.declaration.variable;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class VariableDeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    Visitor<void>& visitor;

  public:
    VariableDeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                               Visitor<void>& visitor)
        : context(context), sema(sema), resolver(resolver), visitor(visitor) {}

    void declare_variable(VarDeclaration& node) {
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

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(attribute_infos, {sema, context, node.span, node.name, {}});

        auto* symbol = sema.env.symbols.create<VariableSymbol>(node.name, node.span,
                                                               node.visibility, node.storage_kind,
                                                               type, std::move(attribute_infos));
        node.variable_symbol = symbol;

        if (!sema.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }

    void declare_extern_variable(ExternVarDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        visitor.visit(*node.annotation);

        const auto* type = resolver.resolve_type(node.annotation->type);
        node.type = type;

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(attribute_infos, {sema, context, node.span, node.name, {}});

        auto* symbol = sema.env.symbols.create<VariableSymbol>(
            node.name, node.span, node.visibility, StorageKind::Type::Var, type,
            std::move(attribute_infos));
        node.variable_symbol = symbol;

        if (!sema.env.current_scope->define_var(node.name, symbol)) {
            context.diagnostics.add_error(node.span,
                                          "redefinition of variable '" + node.name + "'");
        }
    }
};
