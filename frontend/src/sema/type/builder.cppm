module;

#include <vector>

export module zep.frontend.sema.type.builder;

import zep.frontend.sema.type;
import zep.frontend.arena;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.sema.context;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.env;
import zep.frontend.sema.resolver.generic;

export class TypeBuilder {
  private:
    Visitor<void>& visitor;

    Context& context;
    TypeResolver& resolver;

  public:
    explicit TypeBuilder(Visitor<void>& visitor, Context& context, TypeResolver& resolver)
        : visitor(visitor), context(context), resolver(resolver) {}

    std::vector<GenericParameterType>
    build_generic_parameters(std::vector<GenericParameter*>& generic_parameters) {
        std::vector<GenericParameterType> result;
        result.reserve(generic_parameters.size());

        for (auto* generic_parameter : generic_parameters) {
            const Type* constraint = nullptr;
            if (generic_parameter->constraint != nullptr) {
                visitor.visit(*generic_parameter->constraint);
                constraint = generic_parameter->constraint->type;
            }

            result.emplace_back(generic_parameter->name, constraint);
        }

        return result;
    }

    const Type* build_struct(StructDeclaration& node) {
        auto generic_parameter_types = build_generic_parameters(node.generic_parameters);

        auto scope = resolver.scoped_substitutions();
        GenericResolver generic_resolver(context, resolver, visitor);
        generic_resolver.activate_generic_parameters(generic_parameter_types, true);

        std::vector<StructFieldType> field_types;
        field_types.reserve(node.fields.size());

        for (auto* field : node.fields) {
            visitor.visit(*field);
            field_types.emplace_back(field->name, field->type->type);
        }

        return context.types.create<StructType>(node.name, std::move(generic_parameter_types),
                                                std::move(field_types));
    }

    const FunctionType* build_function(FunctionPrototype& prototype) {
        auto generic_parameter_types = build_generic_parameters(prototype.generic_parameters);

        auto scope = resolver.scoped_substitutions();
        GenericResolver generic_resolver(context, resolver, visitor);
        generic_resolver.activate_generic_parameters(generic_parameter_types, true);

        std::vector<ParameterType> parameter_types;
        parameter_types.reserve(prototype.parameters.size());

        bool is_variadic = false;

        for (auto* parameter : prototype.parameters) {
            visitor.visit(*parameter);

            parameter_types.emplace_back(parameter->name, parameter->type->type);

            if (parameter->is_variadic) {
                is_variadic = true;
            }
        }

        visitor.visit(*prototype.return_type);

        return context.types.create<FunctionType>(prototype.name, prototype.return_type->type,
                                                  std::move(parameter_types),
                                                  std::move(generic_parameter_types), is_variadic);
    }
};
