module;

#include <vector>

export module zep.frontend.sema.builder;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.arena.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
export class TypeBuilder {
  private:
    Visitor<void>& visitor;

  public:
    explicit TypeBuilder(Visitor<void>& visitor) : visitor(visitor) {}

    std::vector<GenericParameterType>
    build_generic_parameters(std::vector<GenericParameter*>& generic_parameters) {
        std::vector<GenericParameterType> result;
        result.reserve(generic_parameters.size());

        for (GenericParameter* generic_parameter : generic_parameters) {
            TypeId constraint = TypeId{};
            if (generic_parameter->constraint != nullptr) {
                visitor.visit(*generic_parameter->constraint);
                constraint = generic_parameter->constraint->type;
            }

            result.emplace_back(generic_parameter->name, constraint);
        }

        return result;
    }

    TypeId build_struct(StructDeclaration& node, TypeArena& types) {
        std::vector<GenericParameterType> generic_parameter_types =
            build_generic_parameters(node.generic_parameters);

        std::vector<StructFieldType> field_types;
        field_types.reserve(node.fields.size());

        for (StructField* field : node.fields) {
            visitor.visit(*field);
            field_types.emplace_back(field->name, field->type->type);
        }

        return types.create<StructType>(node.name, std::move(generic_parameter_types),
                                        std::move(field_types));
    }

    TypeId build_function(FunctionPrototype& prototype, TypeArena& types) {
        std::vector<GenericParameterType> generic_parameter_types =
            build_generic_parameters(prototype.generic_parameters);

        bool is_variadic = false;

        std::vector<ParameterType> parameter_types;
        parameter_types.reserve(prototype.parameters.size());

        for (Parameter* parameter : prototype.parameters) {
            visitor.visit(*parameter);
            parameter_types.emplace_back(parameter->name, parameter->type->type);

            if (parameter->is_variadic) {
                is_variadic = true;
            }
        }

        visitor.visit(*prototype.return_type);
        TypeId return_type = prototype.return_type->type;

        return types.create<FunctionType>(prototype.name, return_type, std::move(parameter_types),
                                          std::move(generic_parameter_types), is_variadic);
    }
};
