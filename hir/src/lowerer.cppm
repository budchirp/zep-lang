module;

#include <algorithm>
#include <string>
#include <vector>

export module zep.hir.lowerer;

import zep.common.span;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.context;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;
import zep.hir.node;
import zep.hir.node.program;
import zep.hir.monomorphizer;

export template <typename Builder>
class HIRLowerer {
  private:
    HIRProgram& program;

    TypeResolver& resolver;

    MonomorphizationCache& mono_cache;

    SemaContext& sema;

    Builder& builder;

  public:
    HIRLowerer(HIRProgram& program, TypeResolver& resolver, MonomorphizationCache& mono_cache,
               SemaContext& sema, Builder& builder)
        : program(program), resolver(resolver), mono_cache(mono_cache), sema(sema),
          builder(builder) {}

    void apply_generic_substitutions(const std::vector<GenericParameter*>& generic_parameters,
                                     const std::vector<const Type*>& argument_types) {
        const auto count = std::min(generic_parameters.size(), argument_types.size());

        for (std::size_t index = 0; index < count; ++index) {
            resolver.add_substitution(generic_parameters[index]->name, argument_types[index]);
        }
    }

    std::vector<const Type*>
    lower_generic_arguments(const std::vector<GenericArgumentType>& generic_argument_types,
                            Span span) {
        std::vector<const Type*> argument_types;
        argument_types.reserve(generic_argument_types.size());

        for (const auto& generic_argument_type : generic_argument_types) {
            argument_types.push_back(lower_type(generic_argument_type.type, span));
        }

        return argument_types;
    }

    std::vector<const Type*>
    lower_generic_arguments(const std::vector<GenericArgument*>& generic_arguments, Span span) {
        std::vector<const Type*> argument_types;
        argument_types.reserve(generic_arguments.size());

        for (const auto* generic_argument : generic_arguments) {
            argument_types.push_back(lower_type(generic_argument->type->type, span));
        }

        return argument_types;
    }

    const Type* lower_type(const Type* type, Span span) {
        if (type == nullptr) {
            return nullptr;
        }

        if (const auto* pointer_type = type->template as<PointerType>(); pointer_type != nullptr) {
            return sema.types.create<PointerType>(lower_type(pointer_type->element, span),
                                                  pointer_type->is_mutable);
        }

        if (const auto* array_type = type->template as<ArrayType>(); array_type != nullptr) {
            return sema.types.create<ArrayType>(lower_type(array_type->element, span),
                                                array_type->size);
        }

        if (const auto* function_type = type->template as<FunctionType>();
            function_type != nullptr) {
            std::vector<ParameterType> parameter_types;
            parameter_types.reserve(function_type->parameters.size());

            for (const auto& parameter : function_type->parameters) {
                parameter_types.emplace_back(parameter.name, lower_type(parameter.type, span));
            }

            return sema.types.create<FunctionType>(
                function_type->name, lower_type(function_type->return_type, span),
                std::move(parameter_types), function_type->generic_parameters,
                function_type->variadic);
        }

        const auto* resolved = resolver.resolve(type);

        if (const auto* struct_type = resolved->template as<StructType>(); struct_type != nullptr) {
            return lower_monomorphized_struct(type, struct_type, span);
        }

        return resolved;
    }

    const Type* lower_monomorphized_struct(const Type* type, const StructType* struct_type,
                                           Span span) {
        std::vector<const Type*> argument_types;

        const auto* definition = mono_cache.get_struct(struct_type->name);

        if (const auto* named_type = type->template as<NamedType>();
            named_type != nullptr && !named_type->generic_arguments.empty()) {
            argument_types = lower_generic_arguments(named_type->generic_arguments, span);
        } else if (definition != nullptr) {
            argument_types.reserve(definition->generic_parameters.size());

            for (const auto& generic_parameter : definition->generic_parameters) {
                argument_types.push_back(resolver.resolve(sema.types.create<NamedType>(
                    generic_parameter->name, std::vector<GenericArgumentType>())));
            }
        }

        if (argument_types.empty()) {
            builder.register_type_symbol(struct_type->name, Visibility::Type::Public, struct_type,
                                         span);

            return struct_type;
        }

        const auto instance = mono_cache.get_or_create(struct_type->name, argument_types);

        if (const auto* prior_symbol = program.global_scope->lookup_type(instance.name)) {
            return prior_symbol->type;
        }

        auto* instance_struct = sema.types.create<StructType>(
            instance.name, std::vector<GenericParameterType>(), std::vector<StructFieldType>());

        builder.register_type_symbol(instance.name, Visibility::Type::Public, instance_struct,
                                     span);

        auto scope = resolver.scoped_substitutions();

        if (definition != nullptr) {
            apply_generic_substitutions(definition->generic_parameters, argument_types);
        }

        instance_struct->fields.reserve(struct_type->fields.size());

        for (const auto& field : struct_type->fields) {
            instance_struct->fields.emplace_back(field.name, lower_type(field.type, span));
        }

        return instance_struct;
    }

    HIRFunctionDeclaration*
    lower_monomorphized_function(const FunctionDeclaration& declaration,
                                 const std::string& mangled_name,
                                 const std::vector<const Type*>& argument_types = {}) {
        auto scope = resolver.scoped_substitutions();

        apply_generic_substitutions(declaration.prototype->generic_parameters, argument_types);

        std::vector<HIRParameter> hir_parameters;
        hir_parameters.reserve(declaration.prototype->parameters.size());

        for (const auto& ast_parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(ast_parameter->name,
                                        lower_type(ast_parameter->type->type, declaration.span));
        }

        const auto variadic = !declaration.prototype->parameters.empty() &&
                              declaration.prototype->parameters.back()->is_variadic;

        const auto* return_type =
            lower_type(declaration.prototype->return_type->type, declaration.span);
        const auto* signature =
            lower_type(declaration.type, declaration.span)->template as<FunctionType>();
        const auto* mangled_type =
            sema.types.create<FunctionType>(mangled_name, return_type, signature->parameters,
                                            signature->generic_parameters, variadic);

        auto* body =
            static_cast<HIRBlockStatement*>(builder.visit_statement(*declaration.body));

        return program.context.nodes.template create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::Internal, mangled_name,
            std::move(hir_parameters), return_type, body, variadic, mangled_type);
    }
};
