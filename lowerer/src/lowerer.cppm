module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.lowerer;

import zep.checker.context;
import zep.checker.type_context;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.sema.type;
import zep.sema.kinds;
import zep.lowerer.nodes;
import zep.lowerer.types;
import zep.lowerer.env;
import zep.lowerer.name_mangler;
import zep.lowerer.monomorphizer;

export class Lowerer {
  private:
    Context& context;
    TypeContext type_context;
    Monomorphizer monomorphizer;
    LoweredEnv env;

    std::shared_ptr<LoweredType> sema_to_lowered_type(std::shared_ptr<Type> type) {
        type = type_context.resolve_type(type);

        if (type == nullptr) {
            return nullptr;
        }

        switch (type->kind) {
        case Type::Kind::Any:
            return std::make_shared<LoweredVoidType>();
        case Type::Kind::Unknown:
            context.logger.error("lowerer: UnknownType cannot appear in lowered IR");
        case Type::Kind::Named: {
            auto* named = type->as<NamedType>();
            std::string msg = "lowerer: unresolved NamedType '" + (named ? named->name : "?") +
                              "' cannot appear in lowered IR";
            context.logger.error(msg);
        }

        case Type::Kind::Void:
            return std::make_shared<LoweredVoidType>();
        case Type::Kind::Boolean:
            return std::make_shared<LoweredBooleanType>();
        case Type::Kind::String:
            return std::make_shared<LoweredStringType>();

        case Type::Kind::Integer: {
            auto* integer = type->as<IntegerType>();
            return std::make_shared<LoweredIntegerType>(integer->size, !integer->is_unsigned);
        }

        case Type::Kind::Float: {
            auto* float_type = type->as<FloatType>();
            return std::make_shared<LoweredFloatType>(float_type->size);
        }

        case Type::Kind::Pointer: {
            auto* pointer = type->as<PointerType>();
            auto base = sema_to_lowered_type(pointer->element);
            return std::make_shared<LoweredPointerType>(std::move(base));
        }
        case Type::Kind::Array: {
            auto* array = type->as<ArrayType>();
            auto element = sema_to_lowered_type(array->element);
            if (!array->size.has_value()) {
                return std::make_shared<LoweredPointerType>(std::move(element));
            }
            return std::make_shared<LoweredArrayType>(std::move(element),
                                                      static_cast<std::size_t>(*array->size));
        }

        case Type::Kind::Struct: {
            auto* struct_type = type->as<StructType>();
            return std::make_shared<LoweredStructType>(struct_type->name);
        }

        case Type::Kind::Function: {
            auto* func_type = type->as<FunctionType>();
            std::vector<std::shared_ptr<LoweredType>> lowered_params;
            for (const auto& param : func_type->parameters) {
                lowered_params.push_back(sema_to_lowered_type(param.type));
            }
            auto lowered_return = sema_to_lowered_type(func_type->return_type);
            return std::make_shared<LoweredFunctionType>(
                std::move(lowered_params), std::move(lowered_return), func_type->variadic);
        }
        }

        context.logger.error("lowerer: unhandled type in sema_to_lowered_type");
    }

    std::unique_ptr<LoweredExpression> lower_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>()) {
            auto result = std::make_unique<LoweredNumberLiteral>(node->position, node->value);
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<FloatLiteral>()) {
            auto result = std::make_unique<LoweredFloatLiteral>(node->position, node->value);
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<StringLiteral>()) {
            auto result = std::make_unique<LoweredStringLiteral>(node->position, node->value);
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<BooleanLiteral>()) {
            auto result = std::make_unique<LoweredBooleanLiteral>(node->position, node->value);
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
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
            auto op = static_cast<LoweredBinaryExpression::BinaryOperator>(
                static_cast<std::uint8_t>(node->op));
            auto result = std::make_unique<LoweredBinaryExpression>(
                node->position, std::move(lowered_left), op, std::move(lowered_right));
            result->set_type(sema_to_lowered_type(node->get_type()));
            return result;
        }

        if (auto* node = expression.as<UnaryExpression>()) {
            auto lowered_operand = lower_expression(*node->operand);
            auto op = static_cast<LoweredUnaryExpression::UnaryOperator>(
                static_cast<std::uint8_t>(node->op));
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
            return lower_struct_literal_expression(*node);
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

    std::unique_ptr<LoweredExpression> lower_call_expression(CallExpression& node) {
        auto callee_type = node.callee->get_type();
        auto* func_type = (callee_type != nullptr) ? callee_type->as<FunctionType>() : nullptr;

        std::vector<std::unique_ptr<LoweredExpression>> lowered_arguments;
        for (auto& argument : node.arguments) {
            lowered_arguments.push_back(lower_expression(*argument->value));
        }

        std::unique_ptr<LoweredExpression> lowered_callee;

        std::shared_ptr<Type> callee_type_to_convert = callee_type;
        if (auto* identifier = node.callee->as<IdentifierExpression>()) {
            std::string callee_name = identifier->name;

            if (!monomorphizer.is_extern_function(identifier->name) && func_type != nullptr) {
                if (!node.generic_arguments.empty()) {
                    std::vector<std::shared_ptr<Type>> type_arguments;
                    for (auto& generic_argument : node.generic_arguments) {
                        type_arguments.push_back(generic_argument->type->get_type());
                    }

                    auto scope = type_context.scoped_substitutions();
                    for (std::size_t i = 0;
                         i < type_arguments.size() && i < func_type->generic_parameters.size();
                         ++i) {
                        type_context.add_substitution(func_type->generic_parameters[i].name,
                                                      type_arguments[i]);
                    }

                    std::vector<std::shared_ptr<Type>> resolved_parameter_types;
                    for (const auto& parameter : func_type->parameters) {
                        resolved_parameter_types.push_back(
                            type_context.resolve_type(parameter.type));
                    }

                    auto resolved_return_type = type_context.resolve_type(func_type->return_type);
                    callee_type_to_convert = type_context.resolve_type(callee_type);

                    callee_name = monomorphizer.request_function_specialization(
                        identifier->name, type_arguments, resolved_parameter_types,
                        resolved_return_type, func_type->variadic);
                } else {
                    std::vector<std::shared_ptr<Type>> parameter_types;
                    for (const auto& parameter : func_type->parameters) {
                        parameter_types.push_back(type_context.resolve_type(parameter.type));
                    }
                    auto return_type = type_context.resolve_type(func_type->return_type);
                    auto overloads =
                        context.env.global_scope->lookup_function_overloads(identifier->name);
                    bool has_overloads = overloads.size() > 1;
                    callee_name = NameMangler::mangle_function_if_needed(
                        identifier->name, {}, parameter_types, return_type, func_type->variadic,
                        has_overloads);
                }
            }

            auto lowered_identifier =
                std::make_unique<LoweredIdentifierExpression>(identifier->position, callee_name);
            lowered_identifier->set_type(sema_to_lowered_type(callee_type_to_convert));
            lowered_callee = std::move(lowered_identifier);
        } else {
            lowered_callee = lower_expression(*node.callee);
        }

        auto result = std::make_unique<LoweredCallExpression>(
            node.position, std::move(lowered_callee), std::move(lowered_arguments));
        result->set_type(sema_to_lowered_type(node.get_type()));
        return result;
    }

    std::unique_ptr<LoweredExpression>
    lower_struct_literal_expression(StructLiteralExpression& node) {
        std::string struct_name = node.name->name;

        if (!node.generic_arguments.empty()) {
            std::vector<std::shared_ptr<Type>> type_arguments;
            for (auto& generic_argument : node.generic_arguments) {
                type_arguments.push_back(generic_argument->type->get_type());
            }
            struct_name =
                monomorphizer.request_struct_specialization(node.name->name, type_arguments);
        }

        std::vector<LoweredStructLiteralField> lowered_fields;
        for (auto& field : node.fields) {
            lowered_fields.emplace_back(field->name, lower_expression(*field->value));
        }

        auto result = std::make_unique<LoweredStructLiteralExpression>(
            node.position, std::move(struct_name), std::move(lowered_fields));
        result->set_type(sema_to_lowered_type(node.get_type()));
        return result;
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
                node->name, std::make_unique<LoweredVarSymbol>(node->name, Linkage::Internal,
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
    lower_function_declaration(FunctionDeclaration& declaration) {
        std::vector<LoweredParameter> lowered_parameters;
        std::vector<std::shared_ptr<Type>> parameter_types;

        for (auto& parameter : declaration.prototype->parameters) {
            auto param_type = type_context.resolve_type(parameter->type->get_type());
            auto lowered_param = sema_to_lowered_type(param_type);
            lowered_parameters.emplace_back(parameter->name, lowered_param);
            parameter_types.push_back(param_type);
        }

        auto return_type =
            type_context.resolve_type(declaration.prototype->return_type->get_type());
        auto lowered_return = sema_to_lowered_type(return_type);
        bool variadic = prototype_is_variadic(*declaration.prototype);

        auto overloads =
            context.env.global_scope->lookup_function_overloads(declaration.prototype->name);
        bool has_overloads = overloads.size() > 1;
        auto mangled_name = NameMangler::mangle_function_if_needed(
            declaration.prototype->name, {}, parameter_types, return_type, variadic, has_overloads);

        std::vector<std::shared_ptr<LoweredType>> param_types;
        for (const auto& p : lowered_parameters) {
            param_types.push_back(p.type);
        }
        auto func_type =
            std::make_shared<LoweredFunctionType>(std::move(param_types), lowered_return, variadic);
        auto return_for_decl = func_type->return_type;
        auto symbol = std::make_unique<LoweredFunctionSymbol>(
            mangled_name, Linkage::Internal, declaration.visibility, std::move(func_type));
        env.current_scope->define_function(mangled_name, std::move(symbol));

        env.push_scope(env.current_scope->name + "." + mangled_name);
        for (const auto& param : lowered_parameters) {
            env.current_scope->define_var(
                param.name, std::make_unique<LoweredVarSymbol>(param.name, Linkage::Internal,
                                                               Visibility::Private, param.type));
        }
        auto lowered_body = lower_block_statement(*declaration.body);

        env.pop_scope();

        return std::make_unique<LoweredFunctionDeclaration>(
            declaration.position, declaration.visibility, std::move(mangled_name),
            std::move(lowered_parameters), std::move(return_for_decl), std::move(lowered_body),
            variadic);
    }

    void lower_struct_declaration(StructDeclaration& declaration) {
        std::vector<LoweredStructField> lowered_fields;
        for (auto& field : declaration.fields) {
            auto field_type =
                sema_to_lowered_type(type_context.resolve_type(field->type->get_type()));
            lowered_fields.emplace_back(field->name, std::move(field_type));
        }

        auto struct_type =
            std::make_shared<LoweredStructType>(declaration.name, std::move(lowered_fields));
        auto symbol = std::make_unique<LoweredTypeSymbol>(
            declaration.name, Linkage::Internal, declaration.visibility, std::move(struct_type));
        env.current_scope->define_type(declaration.name, std::move(symbol));
    }

    void lower_extern_function_declaration(ExternFunctionDeclaration& declaration) {
        std::vector<std::shared_ptr<LoweredType>> lowered_params;
        for (auto& parameter : declaration.prototype->parameters) {
            auto param_type =
                sema_to_lowered_type(type_context.resolve_type(parameter->type->get_type()));
            lowered_params.push_back(std::move(param_type));
        }

        auto return_type = sema_to_lowered_type(
            type_context.resolve_type(declaration.prototype->return_type->get_type()));
        bool variadic = prototype_is_variadic(*declaration.prototype);
        auto func_type = std::make_shared<LoweredFunctionType>(std::move(lowered_params),
                                                               std::move(return_type), variadic);

        auto symbol =
            std::make_unique<LoweredFunctionSymbol>(declaration.prototype->name, Linkage::External,
                                                    declaration.visibility, std::move(func_type));
        env.current_scope->define_function(declaration.prototype->name, std::move(symbol));
    }

    void lower_extern_var_declaration(ExternVarDeclaration& declaration) {
        auto var_type =
            sema_to_lowered_type(type_context.resolve_type(declaration.type->get_type()));
        auto symbol = std::make_unique<LoweredVarSymbol>(
            declaration.name, Linkage::External, declaration.visibility, std::move(var_type));
        env.current_scope->define_var(declaration.name, std::move(symbol));
    }

    std::unique_ptr<LoweredFunctionDeclaration>
    specialize_function(FunctionDeclaration& declaration, const std::string& mangled_name,
                        const std::vector<std::shared_ptr<Type>>& type_arguments) {
        auto scope = type_context.scoped_substitutions();

        auto& generic_parameters = declaration.prototype->generic_parameters;
        for (std::size_t i = 0; i < type_arguments.size() && i < generic_parameters.size(); ++i) {
            type_context.add_substitution(generic_parameters[i]->name, type_arguments[i]);
        }

        std::vector<LoweredParameter> lowered_parameters;
        for (auto& parameter : declaration.prototype->parameters) {
            auto param_type =
                sema_to_lowered_type(type_context.resolve_type(parameter->type->get_type()));
            lowered_parameters.emplace_back(parameter->name, std::move(param_type));
        }

        auto return_type = sema_to_lowered_type(
            type_context.resolve_type(declaration.prototype->return_type->get_type()));
        bool variadic = prototype_is_variadic(*declaration.prototype);

        std::vector<std::shared_ptr<LoweredType>> param_types;
        for (const auto& p : lowered_parameters) {
            param_types.push_back(p.type);
        }
        auto func_type =
            std::make_shared<LoweredFunctionType>(std::move(param_types), return_type, variadic);
        auto return_for_decl = func_type->return_type;
        auto symbol = std::make_unique<LoweredFunctionSymbol>(
            mangled_name, Linkage::Internal, declaration.visibility, std::move(func_type));
        env.current_scope->define_function(mangled_name, std::move(symbol));

        env.push_scope(env.current_scope->name + "." + mangled_name);
        for (const auto& param : lowered_parameters) {
            env.current_scope->define_var(
                param.name, std::make_unique<LoweredVarSymbol>(param.name, Linkage::Internal,
                                                               Visibility::Private, param.type));
        }
        auto lowered_body = lower_block_statement(*declaration.body);

        env.pop_scope();

        return std::make_unique<LoweredFunctionDeclaration>(
            declaration.position, declaration.visibility, mangled_name,
            std::move(lowered_parameters), std::move(return_for_decl), std::move(lowered_body),
            variadic);
    }

    void specialize_struct(StructDeclaration& declaration, const std::string& mangled_name,
                           const std::vector<std::shared_ptr<Type>>& type_arguments) {
        auto scope = type_context.scoped_substitutions();

        auto& generic_parameters = declaration.generic_parameters;
        for (std::size_t i = 0; i < type_arguments.size() && i < generic_parameters.size(); ++i) {
            type_context.add_substitution(generic_parameters[i]->name, type_arguments[i]);
        }

        std::vector<LoweredStructField> lowered_fields;
        for (auto& field : declaration.fields) {
            auto field_type =
                sema_to_lowered_type(type_context.resolve_type(field->type->get_type()));
            lowered_fields.emplace_back(field->name, std::move(field_type));
        }

        auto struct_type =
            std::make_shared<LoweredStructType>(mangled_name, std::move(lowered_fields));
        auto symbol = std::make_unique<LoweredTypeSymbol>(
            mangled_name, Linkage::Internal, declaration.visibility, std::move(struct_type));
        env.current_scope->define_type(mangled_name, std::move(symbol));
    }

  public:
    explicit Lowerer(Context& context) : context(context), type_context(context.env) {}

    LoweredProgram lower(Program& program) {
        monomorphizer.index_declarations(program);

        std::vector<std::unique_ptr<LoweredFunctionDeclaration>> functions;

        for (auto& statement : program.statements) {
            if (auto* func = statement->as<FunctionDeclaration>()) {
                if (!func->prototype->generic_parameters.empty()) {
                    continue;
                }
                functions.push_back(lower_function_declaration(*func));
            } else if (auto* struct_declaration = statement->as<StructDeclaration>()) {
                if (!struct_declaration->generic_parameters.empty()) {
                    continue;
                }
                lower_struct_declaration(*struct_declaration);
            } else if (auto* extern_func = statement->as<ExternFunctionDeclaration>()) {
                lower_extern_function_declaration(*extern_func);
            } else if (auto* extern_var = statement->as<ExternVarDeclaration>()) {
                lower_extern_var_declaration(*extern_var);
            }
        }

        while (monomorphizer.has_pending()) {
            auto pending = monomorphizer.pop_pending();

            if (pending.kind == Monomorphizer::PendingSpecialization::Kind::Function) {
                auto* declaration = monomorphizer.get_function_declaration(pending.original_name);
                if (declaration != nullptr) {
                    functions.push_back(specialize_function(*declaration, pending.mangled_name,
                                                            pending.type_arguments));
                }
            } else {
                auto* declaration = monomorphizer.get_struct_declaration(pending.original_name);
                if (declaration != nullptr) {
                    specialize_struct(*declaration, pending.mangled_name, pending.type_arguments);
                }
            }
        }

        return LoweredProgram(std::move(env.global_scope), std::move(functions));
    }
};
