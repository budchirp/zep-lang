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

    void apply_generic_substitutions(const std::vector<GenericParameter*>& parameters,
                                     const std::vector<const Type*>& arguments) {
        const auto min_size = std::min(parameters.size(), arguments.size());
        for (std::size_t i = 0; i < min_size; ++i) {
            resolver.add_substitution(parameters[i]->name, arguments[i]);
        }
    }

    std::vector<const Type*>
    lower_generic_arguments(const std::vector<GenericArgumentType>& arguments, Span span) {
        std::vector<const Type*> generic_args;
        generic_args.reserve(arguments.size());
        for (const auto& arg : arguments) {
            generic_args.push_back(lower_type(arg.type, span));
        }
        return generic_args;
    }

    std::vector<const Type*> lower_generic_arguments(const std::vector<GenericArgument*>& arguments,
                                                     Span span) {
        std::vector<const Type*> generic_args;
        generic_args.reserve(arguments.size());
        for (const auto* arg : arguments) {
            generic_args.push_back(lower_type(arg->type->type, span));
        }
        return generic_args;
    }

    const Type* lower_type(const Type* type, Span span) {
        if (type == nullptr) {
            return nullptr;
        }

        if (const auto* pointer = type->template as<PointerType>(); pointer != nullptr) {
            return sema.types.create<PointerType>(lower_type(pointer->element, span),
                                                  pointer->is_mutable);
        }

        if (const auto* array = type->template as<ArrayType>(); array != nullptr) {
            return sema.types.create<ArrayType>(lower_type(array->element, span), array->size);
        }

        if (const auto* function = type->template as<FunctionType>(); function != nullptr) {
            std::vector<ParameterType> parameters;
            parameters.reserve(function->parameters.size());
            for (const auto& param : function->parameters) {
                parameters.emplace_back(param.name, lower_type(param.type, span));
            }
            return sema.types.create<FunctionType>(
                function->name, lower_type(function->return_type, span), std::move(parameters),
                function->generic_parameters, function->variadic);
        }

        const auto* resolved = resolver.resolve(type);

        if (const auto* struct_type = resolved->template as<StructType>(); struct_type != nullptr) {
            return lower_monomorphized_struct(type, struct_type, span);
        }

        return resolved;
    }

    const Type* lower_monomorphized_struct(const Type* type, const StructType* struct_type,
                                           Span span) {
        std::vector<const Type*> generic_args;

        if (const auto* named = type->template as<NamedType>();
            named != nullptr && !named->generic_arguments.empty()) {
            generic_args = lower_generic_arguments(named->generic_arguments, span);
        } else if (const auto* original = mono_cache.get_struct(struct_type->name)) {
            generic_args.reserve(original->generic_parameters.size());
            for (const auto& param : original->generic_parameters) {
                generic_args.push_back(resolver.resolve(
                    sema.types.create<NamedType>(param->name, std::vector<GenericArgumentType>())));
            }
        }

        if (!generic_args.empty()) {
            auto result = mono_cache.get_or_create(struct_type->name, generic_args);
            if (auto* existing = program.global_scope->lookup_type(result.name)) {
                return existing->type;
            }

            auto* mangled_struct_type = sema.types.create<StructType>(
                result.name, std::vector<GenericParameterType>(), std::vector<StructFieldType>());
            builder.register_type_symbol(result.name, Visibility::Type::Public, mangled_struct_type, span);

            auto sub_scope = resolver.scoped_substitutions();
            if (const auto* original = mono_cache.get_struct(struct_type->name)) {
                apply_generic_substitutions(original->generic_parameters, generic_args);
            }

            mangled_struct_type->fields.reserve(struct_type->fields.size());
            for (const auto& field : struct_type->fields) {
                mangled_struct_type->fields.emplace_back(field.name, lower_type(field.type, span));
            }

            return mangled_struct_type;
        }

        builder.register_type_symbol(struct_type->name, Visibility::Type::Public, struct_type, span);
        return struct_type;
    }

    HIRFunctionDeclaration*
    lower_monomorphized_function(const FunctionDeclaration& declaration,
                                 const std::string& mangled_name,
                                 const std::vector<const Type*>& generic_arguments = {}) {
        auto sub_scope = resolver.scoped_substitutions();
        apply_generic_substitutions(declaration.prototype->generic_parameters, generic_arguments);

        std::vector<HIRParameter> hir_parameters;
        hir_parameters.reserve(declaration.prototype->parameters.size());

        for (const auto& parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name,
                                        lower_type(parameter->type->type, declaration.span));
        }

        auto is_variadic = !declaration.prototype->parameters.empty() &&
                           declaration.prototype->parameters.back()->is_variadic;

        const auto* return_type =
            lower_type(declaration.prototype->return_type->type, declaration.span);
        const auto* function_type =
            lower_type(declaration.type, declaration.span)->template as<FunctionType>();

        auto* mangled_type =
            sema.types.create<FunctionType>(mangled_name, return_type, function_type->parameters,
                                            function_type->generic_parameters, is_variadic);

        return program.context.arena.template create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::Internal, mangled_name,
            std::move(hir_parameters), return_type,
            static_cast<HIRBlockStatement*>(builder.lower_statement(*declaration.body)), is_variadic,
            mangled_type);
    }
};
