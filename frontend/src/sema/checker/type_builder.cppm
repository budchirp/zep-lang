module;

#include <memory>
#include <vector>

export module zep.frontend.sema.checker.type_builder;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;

export class TypeBuilder {
  private:
    Visitor<void>& visitor;

  public:
    explicit TypeBuilder(Visitor<void>& visitor) : visitor(visitor) {}

    std::vector<std::shared_ptr<GenericParameterType>>
    build_generic_parameters(std::vector<std::unique_ptr<GenericParameter>>& generic_parameters) {
        std::vector<std::shared_ptr<GenericParameterType>> result;
        result.reserve(generic_parameters.size());

        for (auto& generic_parameter : generic_parameters) {
            std::shared_ptr<Type> constraint;
            if (generic_parameter->constraint != nullptr) {
                visitor.visit(*generic_parameter->constraint);
                constraint = generic_parameter->constraint->get_type();
            }

            result.emplace_back(std::make_shared<GenericParameterType>(generic_parameter->name,
                                                                       std::move(constraint)));
        }

        return result;
    }

    std::shared_ptr<StructType> build_struct(StructDeclaration& node) {
        auto generic_parameter_types = build_generic_parameters(node.generic_parameters);

        std::vector<std::shared_ptr<StructFieldType>> field_types;
        field_types.reserve(node.fields.size());

        for (auto& field : node.fields) {
            visitor.visit(*field);
            field_types.emplace_back(
                std::make_shared<StructFieldType>(field->name, field->type->get_type()));
        }

        return std::make_shared<StructType>(node.name, std::move(generic_parameter_types),
                                            std::move(field_types));
    }

    std::shared_ptr<FunctionType> build_function(FunctionPrototype& prototype) {
        auto generic_parameter_types = build_generic_parameters(prototype.generic_parameters);

        bool is_variadic = false;

        std::vector<std::shared_ptr<ParameterType>> parameter_types;
        parameter_types.reserve(prototype.parameters.size());

        for (auto& parameter : prototype.parameters) {
            visitor.visit(*parameter);
            parameter_types.emplace_back(
                std::make_shared<ParameterType>(parameter->name, parameter->type->get_type()));

            if (parameter->is_variadic) {
                is_variadic = true;
            }
        }

        visitor.visit(*prototype.return_type);
        auto return_type = prototype.return_type->get_type();

        return std::make_shared<FunctionType>(prototype.name, return_type,
                                              std::move(parameter_types),
                                              std::move(generic_parameter_types), is_variadic);
    }
};
