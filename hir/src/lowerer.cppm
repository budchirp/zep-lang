module;

#include <memory>
#include <string>
#include <utility>
#include <vector>

export module zep.lowerer;

import zep.sema.context;
import zep.sema.type.type_context;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.sema.type;
import zep.sema.kinds;
import zep.lowerer.ast.node;
import zep.lowerer.sema.type;
import zep.lowerer.sema.env;
import zep.lowerer.mangler;
import zep.lowerer.monomorphizer;

export class Lowerer {
  private:
    Context& context;
    TypeContext type_context;
    MonomorphizationCache mono_cache;
    std::vector<std::unique_ptr<LoweredFunctionDeclaration>> specialization_buffer;
    LoweredEnv env;

    std::shared_ptr<LoweredType> sema_to_lowered_type(std::shared_ptr<Type> type) {
        type = type_context.resolve_type(type);

        if (type == nullptr) {
            return nullptr;
        }

        switch (type->kind) {
        case Type::Kind::Type::Any:
            return std::make_shared<LoweredVoidType>();
        case Type::Kind::Type::Unknown:
            context.logger.error("lowerer: UnknownType cannot appear in lowered IR");
            return nullptr;
        case Type::Kind::Type::Named: {
            auto* named = type->as<NamedType>();
            std::string msg = "lowerer: unresolved NamedType '" + (named ? named->name : "?") +
                              "' cannot appear in lowered IR";
            context.logger.error(msg);
            return nullptr;
        }

        case Type::Kind::Type::Void:
            return std::make_shared<LoweredVoidType>();
        case Type::Kind::Type::Boolean:
            return std::make_shared<LoweredBooleanType>();
        case Type::Kind::Type::String:
            return std::make_shared<LoweredStringType>();

        case Type::Kind::Type::Integer: {
            auto* integer = type->as<IntegerType>();
            return std::make_shared<LoweredIntegerType>(integer->size, !integer->is_unsigned);
        }

        case Type::Kind::Type::Float: {
            auto* float_type = type->as<FloatType>();
            return std::make_shared<LoweredFloatType>(float_type->size);
        }

        case Type::Kind::Type::Pointer: {
            auto* pointer = type->as<PointerType>();
            auto base = sema_to_lowered_type(pointer->element);
            return std::make_shared<LoweredPointerType>(std::move(base));
        }

        case Type::Kind::Type::Array: {
            auto* array = type->as<ArrayType>();
            auto element = sema_to_lowered_type(array->element);
            if (!array->size.has_value()) {
                return std::make_shared<LoweredPointerType>(std::move(element));
            }
            return std::make_shared<LoweredArrayType>(std::move(element),
                                                      static_cast<std::size_t>(*array->size));
        }

        case Type::Kind::Type::Struct: {
            auto* struct_type = type->as<StructType>();
            return std::make_shared<LoweredStructType>(struct_type->name);
        }

        case Type::Kind::Type::Function: {
            auto* func_type = type->as<FunctionType>();
            std::vector<std::shared_ptr<LoweredType>> lowered_params;
            lowered_params.reserve(func_type->parameters.size());
            for (const auto& param : func_type->parameters) {
                lowered_params.push_back(sema_to_lowered_type(param->type));
            }
            auto lowered_return = sema_to_lowered_type(func_type->return_type);
            return std::make_shared<LoweredFunctionType>(
                std::move(lowered_params), std::move(lowered_return), func_type->variadic);
        }
        }

        context.logger.error("lowerer: unhandled type in sema_to_lowered_type");
        return nullptr;
    }

    static std::vector<std::shared_ptr<Type>>
    call_parameter_types_for_mangle(FunctionType& function_type, TypeContext& type_context) {
        std::size_t parameter_count = function_type.parameters.size();
        if (function_type.variadic && parameter_count > 0) {
            parameter_count -= 1;
        }

        std::vector<std::shared_ptr<Type>> parameter_types;
        parameter_types.reserve(parameter_count);
        for (std::size_t index = 0; index < parameter_count; ++index) {
            parameter_types.push_back(
                type_context.resolve_type(function_type.parameters[index]->type));
        }
        return parameter_types;
    }

    std::string lower_monomorphized_call(CallExpression& expression,
                                         IdentifierExpression& identifier,
                                         FunctionType& function_type,
                                         std::shared_ptr<Type>& callee_type_out) {
        std::vector<std::shared_ptr<Type>> generic_arguments;
        generic_arguments.reserve(expression.generic_arguments.size());
        for (const auto& generic_argument : expression.generic_arguments) {
            generic_arguments.push_back(generic_argument->type->get_type());
        }

        auto substitution_scope = type_context.scoped_substitutions();
        for (std::size_t index = 0;
             index < generic_arguments.size() && index < function_type.generic_parameters.size();
             ++index) {
            type_context.add_substitution(function_type.generic_parameters[index]->name,
                                          generic_arguments[index]);
        }

        callee_type_out = type_context.resolve_type(expression.callee->get_type());

        MonoCacheResult cache_result = mono_cache.get_or_create(identifier.name, generic_arguments);
        if (!cache_result.is_generated) {
            FunctionDeclaration* declaration = mono_cache.get_function(identifier.name);
            if (declaration != nullptr) {
                specialization_buffer.push_back(lower_monomorphized_function(
                    *declaration, cache_result.name, generic_arguments));
                mono_cache.mark_generated(cache_result.name);
            }
        }

        return cache_result.name;
    }

    std::string lower_ordinary_call_callee(IdentifierExpression& identifier,
                                           FunctionType& function_type) {
        std::vector<std::shared_ptr<Type>> parameter_types =
            call_parameter_types_for_mangle(function_type, type_context);
        return NameMangler::mangle(identifier.name, parameter_types);
    }

    template <typename AstLiteral, typename LoweredLiteral>
    std::unique_ptr<LoweredExpression> lower_literal(AstLiteral& node) {
        auto result = std::make_unique<LoweredLiteral>(node.position, node.value);
        result->set_type(sema_to_lowered_type(node.get_type()));
        return result;
    }

    std::unique_ptr<LoweredExpression> try_lower_literal_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>()) {
            return lower_literal<NumberLiteral, LoweredNumberLiteral>(*node);
        }
        if (auto* node = expression.as<FloatLiteral>()) {
            return lower_literal<FloatLiteral, LoweredFloatLiteral>(*node);
        }
        if (auto* node = expression.as<StringLiteral>()) {
            return lower_literal<StringLiteral, LoweredStringLiteral>(*node);
        }
        if (auto* node = expression.as<BooleanLiteral>()) {
            return lower_literal<BooleanLiteral, LoweredBooleanLiteral>(*node);
        }
        return nullptr;
    }

    std::unique_ptr<LoweredExpression> lower_expression(Expression& expression) {
        if (auto literal = try_lower_literal_expression(expression)) {
            return literal;
        }

        if (auto* node = expression.as<IdentifierExpression>()) {
            auto result = std::make_unique<LoweredIdentifierExpression>(node->position, node->name);
            std::shared_ptr<LoweredType> lowered_type;
            const auto* var_symbol = env.current_scope->lookup_var(node->name);
            if (var_symbol != nullptr) {
                lowered_type = var_symbol->type;
            } else {
                lowered_type = sema_to_lowered_type(node->get_type());
            }
            result->set_type(std::move(lowered_type));
            return result;
        }

        if (auto* node = expression.as<BinaryExpression>()) {
            auto lowered_left = lower_expression(*node->left);
            auto lowered_right = lower_expression(*node->right);
            auto op = static_cast<LoweredBinaryOperator::Type>(static_cast<std::uint8_t>(node->op));
            auto result = std::make_unique<LoweredBinaryExpression>(
                node->position, std::move(lowered_left), op, std::move(lowered_right));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<UnaryExpression>()) {
            auto lowered_operand = lower_expression(*node->operand);
            auto op = static_cast<LoweredUnaryOperator::Type>(static_cast<std::uint8_t>(node->op));
            auto result = std::make_unique<LoweredUnaryExpression>(node->position, op,
                                                                   std::move(lowered_operand));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<CallExpression>()) {
            return lower_call_expression(*node);
        }

        if (auto* node = expression.as<IndexExpression>()) {
            auto lowered_object = lower_expression(*node->value);
            auto lowered_index = lower_expression(*node->index);
            auto result = std::make_unique<LoweredIndexExpression>(
                node->position, std::move(lowered_object), std::move(lowered_index));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<MemberExpression>()) {
            auto lowered_object = lower_expression(*node->value);
            auto result = std::make_unique<LoweredMemberExpression>(
                node->position, std::move(lowered_object), node->member);
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<AssignExpression>()) {
            auto lowered_target = lower_expression(*node->target);
            auto lowered_value = lower_expression(*node->value);
            auto result = std::make_unique<LoweredAssignExpression>(
                node->position, std::move(lowered_target), std::move(lowered_value));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<StructLiteralExpression>()) {
            return lower_struct_literal(*node);
        }

        if (auto* node = expression.as<IfExpression>()) {
            auto lowered_condition = lower_expression(*node->condition);
            auto lowered_then = lower_statement(*node->then_branch);
            std::unique_ptr<LoweredStatement> lowered_else;
            if (node->else_branch != nullptr) {
                lowered_else = lower_statement(*node->else_branch);
            }
            auto result = std::make_unique<LoweredIfExpression>(
                node->position, std::move(lowered_condition), std::move(lowered_then),
                std::move(lowered_else));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        return nullptr;
    }

    std::unique_ptr<LoweredExpression> lower_call_expression(CallExpression& expression) {
        auto callee_type = expression.callee->get_type();
        auto* function_type = (callee_type != nullptr) ? callee_type->as<FunctionType>() : nullptr;

        std::vector<std::unique_ptr<LoweredExpression>> lowered_arguments;
        lowered_arguments.reserve(expression.arguments.size());
        for (auto& argument : expression.arguments) {
            lowered_arguments.push_back(lower_expression(*argument->value));
        }

        std::unique_ptr<LoweredExpression> lowered_callee;

        if (auto* identifier = expression.callee->as<IdentifierExpression>()) {
            std::shared_ptr<Type> callee_type_to_convert = callee_type;
            std::string callee_name;

            if (function_type != nullptr && !expression.generic_arguments.empty() &&
                mono_cache.is_generic_function(identifier->name)) {
                callee_name = lower_monomorphized_call(expression, *identifier, *function_type,
                                                       callee_type_to_convert);
            } else if (function_type != nullptr) {
                callee_name = lower_ordinary_call_callee(*identifier, *function_type);
            } else {
                callee_name = identifier->name;
            }

            auto lowered_identifier = std::make_unique<LoweredIdentifierExpression>(
                identifier->position, std::move(callee_name));
            lowered_identifier->set_type(sema_to_lowered_type(callee_type_to_convert));
            lowered_callee = std::move(lowered_identifier);
        } else {
            lowered_callee = lower_expression(*expression.callee);
        }

        auto result = std::make_unique<LoweredCallExpression>(
            expression.position, std::move(lowered_callee), std::move(lowered_arguments));
        result->set_type(sema_to_lowered_type(expression.get_type()));
        return result;
    }

    std::string lower_monomorphized_struct_literal(StructLiteralExpression& literal,
                                                   const std::string& name) {
        std::vector<std::shared_ptr<Type>> generic_arguments;
        generic_arguments.reserve(literal.generic_arguments.size());
        for (const auto& generic_argument : literal.generic_arguments) {
            generic_arguments.push_back(generic_argument->type->get_type());
        }

        MonoCacheResult cache_result = mono_cache.get_or_create(name, generic_arguments);
        if (!cache_result.is_generated) {
            StructDeclaration* declaration = mono_cache.get_struct(name);
            if (declaration != nullptr) {
                auto substitution_scope = type_context.scoped_substitutions();
                for (std::size_t index = 0; index < generic_arguments.size() &&
                                            index < declaration->generic_parameters.size();
                     ++index) {
                    type_context.add_substitution(declaration->generic_parameters[index]->name,
                                                  generic_arguments[index]);
                }

                lower_struct_definition(*declaration, cache_result.name, *env.global_scope);
                mono_cache.mark_generated(cache_result.name);
            }
        }

        return cache_result.name;
    }

    std::unique_ptr<LoweredStructLiteralExpression> lower_struct(StructLiteralExpression& literal,
                                                                 std::string resolved_name) {
        std::vector<LoweredStructLiteralField> lowered_fields;
        lowered_fields.reserve(literal.fields.size());
        for (auto& field : literal.fields) {
            lowered_fields.emplace_back(field->name, lower_expression(*field->value));
        }

        auto result = std::make_unique<LoweredStructLiteralExpression>(
            literal.position, std::move(resolved_name), std::move(lowered_fields));
        result->set_type(sema_to_lowered_type(literal.get_type()));
        return result;
    }

    std::unique_ptr<LoweredExpression> lower_struct_literal(StructLiteralExpression& literal) {
        std::string struct_name;

        if (!literal.generic_arguments.empty() &&
            mono_cache.is_generic_struct(literal.name->name)) {
            struct_name = lower_monomorphized_struct_literal(literal, literal.name->name);
        } else {
            struct_name = literal.name->name;
        }

        return lower_struct(literal, std::move(struct_name));
    }

    std::unique_ptr<LoweredStatement> lower_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>()) {
            return lower_block_statement(*node);
        }

        if (auto* node = statement.as<ExpressionStatement>()) {
            auto lowered_expression = lower_expression(*node->expression);
            auto result = std::make_unique<LoweredExpressionStatement>(
                node->position, std::move(lowered_expression));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = statement.as<ReturnStatement>()) {
            std::unique_ptr<LoweredExpression> lowered_value;
            if (node->value) {
                lowered_value = lower_expression(*node->value);
            }
            auto result =
                std::make_unique<LoweredReturnStatement>(node->position, std::move(lowered_value));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = statement.as<VarDeclaration>()) {
            std::unique_ptr<LoweredExpression> lowered_initializer;
            if (node->initializer) {
                lowered_initializer = lower_expression(*node->initializer);
            }
            auto var_type = sema_to_lowered_type(type_context.resolve_type(node->get_type()));
            env.current_scope->define_var(
                node->name, std::make_unique<LoweredVarSymbol>(node->name, Linkage::Type::Internal,
                                                               node->visibility, var_type));
            auto result = std::make_unique<LoweredVarDeclaration>(
                node->position, node->visibility, node->storage_kind, node->name, var_type,
                std::move(lowered_initializer));
            result->set_type(var_type);
            return result;
        }

        return nullptr;
    }

    std::unique_ptr<LoweredBlockStatement> lower_block_statement(BlockStatement& block) {
        auto* scope = env.current_scope;
        env.push_scope(scope->name + ".block." + std::to_string(scope->child_count()));

        std::vector<std::unique_ptr<LoweredStatement>> lowered_statements;
        for (auto& stmt : block.statements) {
            auto lowered = lower_statement(*stmt);
            if (lowered != nullptr) {
                lowered_statements.push_back(std::move(lowered));
            }
        }

        auto result =
            std::make_unique<LoweredBlockStatement>(block.position, std::move(lowered_statements));
        result->set_type(sema_to_lowered_type(block.get_type()));

        env.pop_scope();
        return result;
    }

    bool prototype_is_variadic(const FunctionPrototype& prototype) {
        for (const auto& parameter : prototype.parameters) {
            if (parameter->is_variadic) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<LoweredFunctionDeclaration>
    lower_function(FunctionDeclaration& declaration, std::string mangled_name,
                   std::vector<LoweredParameter> lowered_parameters,
                   std::shared_ptr<LoweredType> lowered_return, bool variadic) {
        std::vector<std::shared_ptr<LoweredType>> param_types;
        param_types.reserve(lowered_parameters.size());
        for (const auto& param : lowered_parameters) {
            param_types.push_back(param.type);
        }

        auto func_type = std::make_shared<LoweredFunctionType>(std::move(param_types),
                                                               std::move(lowered_return), variadic);
        auto return_for_decl = func_type->return_type;
        auto symbol = std::make_unique<LoweredFunctionSymbol>(
            mangled_name, Linkage::Type::Internal, declaration.visibility, std::move(func_type));
        env.current_scope->define_function(mangled_name, std::move(symbol));

        env.push_scope(env.current_scope->name + "." + mangled_name);
        for (const auto& param : lowered_parameters) {
            env.current_scope->define_var(param.name, std::make_unique<LoweredVarSymbol>(
                                                          param.name, Linkage::Type::Internal,
                                                          Visibility::Type::Private, param.type));
        }

        auto lowered_body = lower_block_statement(*declaration.body);
        env.pop_scope();

        return std::make_unique<LoweredFunctionDeclaration>(
            declaration.position, declaration.visibility, std::move(mangled_name),
            std::move(lowered_parameters), std::move(return_for_decl), std::move(lowered_body),
            variadic);
    }

    std::unique_ptr<LoweredFunctionDeclaration>
    lower_function_declaration(FunctionDeclaration& declaration) {
        std::vector<std::shared_ptr<Type>> mangling_parameter_types;
        std::vector<LoweredParameter> lowered_parameters;
        mangling_parameter_types.reserve(declaration.prototype->parameters.size());
        lowered_parameters.reserve(declaration.prototype->parameters.size());

        for (auto& parameter : declaration.prototype->parameters) {
            auto param_type = type_context.resolve_type(parameter->type->get_type());
            lowered_parameters.emplace_back(parameter->name, sema_to_lowered_type(param_type));
            if (!parameter->is_variadic) {
                mangling_parameter_types.push_back(param_type);
            }
        }

        auto return_type_sema =
            type_context.resolve_type(declaration.prototype->return_type->get_type());
        auto lowered_return = sema_to_lowered_type(return_type_sema);
        bool variadic = prototype_is_variadic(*declaration.prototype);

        std::string mangled_name =
            NameMangler::mangle(declaration.prototype->name, mangling_parameter_types);

        return lower_function(declaration, std::move(mangled_name), std::move(lowered_parameters),
                              std::move(lowered_return), variadic);
    }

    std::vector<LoweredStructField> lower_struct_fields(StructDeclaration& declaration) {
        std::vector<LoweredStructField> lowered_fields;
        lowered_fields.reserve(declaration.fields.size());
        for (auto& field : declaration.fields) {
            auto field_type =
                sema_to_lowered_type(type_context.resolve_type(field->type->get_type()));
            lowered_fields.emplace_back(field->name, std::move(field_type));
        }
        return lowered_fields;
    }

    void lower_struct_definition(StructDeclaration& declaration, const std::string& type_name,
                                 LoweredScope& into_scope) {
        auto lowered_fields = lower_struct_fields(declaration);

        auto struct_type =
            std::make_shared<LoweredStructType>(type_name, std::move(lowered_fields));
        auto symbol = std::make_unique<LoweredTypeSymbol>(
            type_name, Linkage::Type::Internal, declaration.visibility, std::move(struct_type));
        into_scope.define_type(type_name, std::move(symbol));
    }

    void lower_struct_statement(StructDeclaration& declaration) {
        if (!declaration.generic_parameters.empty()) {
            mono_cache.register_struct(declaration.name, &declaration);
            return;
        }

        lower_struct_definition(declaration, declaration.name, *env.current_scope);
    }

    void lower_extern_function_declaration(ExternFunctionDeclaration& declaration) {
        std::vector<std::shared_ptr<Type>> mangling_parameter_types;
        mangling_parameter_types.reserve(declaration.prototype->parameters.size());
        std::vector<std::shared_ptr<LoweredType>> lowered_params;
        for (auto& parameter : declaration.prototype->parameters) {
            auto resolved = type_context.resolve_type(parameter->type->get_type());
            lowered_params.push_back(sema_to_lowered_type(resolved));
            if (!parameter->is_variadic) {
                mangling_parameter_types.push_back(resolved);
            }
        }

        auto return_type = sema_to_lowered_type(
            type_context.resolve_type(declaration.prototype->return_type->get_type()));
        bool variadic = prototype_is_variadic(*declaration.prototype);
        auto func_type = std::make_shared<LoweredFunctionType>(std::move(lowered_params),
                                                               std::move(return_type), variadic);

        std::string mangled_name =
            NameMangler::mangle(declaration.prototype->name, mangling_parameter_types);

        auto symbol = std::make_unique<LoweredFunctionSymbol>(
            mangled_name, Linkage::Type::External, declaration.visibility, std::move(func_type));
        env.current_scope->define_function(mangled_name, std::move(symbol));
    }

    void lower_extern_var_declaration(ExternVarDeclaration& declaration) {
        auto var_type =
            sema_to_lowered_type(type_context.resolve_type(declaration.type->get_type()));
        auto symbol = std::make_unique<LoweredVarSymbol>(
            declaration.name, Linkage::Type::External, declaration.visibility, std::move(var_type));
        env.current_scope->define_var(declaration.name, std::move(symbol));
    }

    std::unique_ptr<LoweredFunctionDeclaration>
    lower_monomorphized_function(FunctionDeclaration& declaration, const std::string& mangled_name,
                                 const std::vector<std::shared_ptr<Type>>& generic_arguments) {
        auto substitution_scope = type_context.scoped_substitutions();
        for (std::size_t index = 0; index < generic_arguments.size() &&
                                    index < declaration.prototype->generic_parameters.size();
             ++index) {
            type_context.add_substitution(declaration.prototype->generic_parameters[index]->name,
                                          generic_arguments[index]);
        }

        std::vector<LoweredParameter> lowered_parameters;
        lowered_parameters.reserve(declaration.prototype->parameters.size());

        for (auto& parameter : declaration.prototype->parameters) {
            auto param_type = type_context.resolve_type(parameter->type->get_type());
            lowered_parameters.emplace_back(parameter->name, sema_to_lowered_type(param_type));
        }

        auto return_type_sema =
            type_context.resolve_type(declaration.prototype->return_type->get_type());
        auto lowered_return = sema_to_lowered_type(return_type_sema);
        bool variadic = prototype_is_variadic(*declaration.prototype);

        return lower_function(declaration, std::string(mangled_name), std::move(lowered_parameters),
                              std::move(lowered_return), variadic);
    }

    std::unique_ptr<LoweredFunctionDeclaration>
    lower_function_statement(FunctionDeclaration& declaration) {
        if (!declaration.prototype->generic_parameters.empty()) {
            mono_cache.register_function(declaration.prototype->name, &declaration);
            return nullptr;
        }

        return lower_function_declaration(declaration);
    }

  public:
    explicit Lowerer(Context& context)
        : context(context), type_context(context.env), mono_cache(), specialization_buffer(),
          env() {}

    LoweredProgram lower(Program& program) {
        specialization_buffer.clear();

        std::vector<std::unique_ptr<LoweredFunctionDeclaration>> functions;

        for (auto& statement : program.statements) {
            if (auto* func = statement->as<FunctionDeclaration>()) {
                if (std::unique_ptr<LoweredFunctionDeclaration> lowered =
                        lower_function_statement(*func)) {
                    functions.push_back(std::move(lowered));
                }
            } else if (auto* struct_declaration = statement->as<StructDeclaration>()) {
                lower_struct_statement(*struct_declaration);
            } else if (auto* extern_func = statement->as<ExternFunctionDeclaration>()) {
                lower_extern_function_declaration(*extern_func);
            } else if (auto* extern_var = statement->as<ExternVarDeclaration>()) {
                lower_extern_var_declaration(*extern_var);
            }
        }

        for (auto& specialized : specialization_buffer) {
            functions.push_back(std::move(specialized));
        }
        specialization_buffer.clear();

        return LoweredProgram(std::move(env.global_scope), std::move(functions));
    }
};
