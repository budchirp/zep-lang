module;

#include <memory>
#include <string>
#include <utility>
#include <vector>

export module zep.hir;

import zep.frontend.sema.context;
import zep.frontend.sema.type.resolver;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.mangler;
import zep.frontend.sema.env;
import zep.hir.ir;
import zep.hir.monomorphizer;

export class HIRBuilder {
  private:
    HIRArena& arena;
    TypeResolver resolver;
    MonomorphizationCache mono_cache;


    std::string lower_monomorphized_call(CallExpression& expression,
                                         IdentifierExpression& identifier,
                                         const Type*& callee_type_out) {
        auto generic_arguments = std::vector<const Type*>();
        generic_arguments.reserve(expression.generic_arguments.size());

        for (const auto& generic_argument : expression.generic_arguments) {
            generic_arguments.push_back(resolver.resolve_type(generic_argument->type->type));
        }

        auto cache_result = mono_cache.get_or_create(identifier.name, generic_arguments);
        
        if (!cache_result.is_generated) {
            const auto* declaration = mono_cache.get_function(identifier.name);

            if (declaration != nullptr) {
                auto* specialization = lower_monomorphized_function(
                    const_cast<FunctionDeclaration&>(*declaration), 
                    cache_result.name,
                    generic_arguments
                );

                mono_cache.enqueue_specialization(specialization);
            }
        }

        callee_type_out = resolver.resolve_type(expression.callee->type);

        return cache_result.name;
    }

    std::string lower_ordinary_call_callee(IdentifierExpression& identifier,
                                           const FunctionType* function_type) {
        auto parameter_types = std::vector<const Type*>();
        parameter_types.reserve(function_type->parameters.size());

        for (const auto& parameter : function_type->parameters) {
            parameter_types.push_back(resolver.resolve_type(parameter.type));
        }

        return NameMangler::mangle(identifier.name, parameter_types);
    }

    HIRExpression* lower_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>(); node != nullptr) {
            return arena.create<HIRNumberLiteral>(node->span, node->value, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<FloatLiteral>(); node != nullptr) {
            return arena.create<HIRFloatLiteral>(node->span, node->value, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<StringLiteral>(); node != nullptr) {
            return arena.create<HIRStringLiteral>(node->span, node->value, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<BooleanLiteral>(); node != nullptr) {
            return arena.create<HIRBooleanLiteral>(node->span, node->value, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<IdentifierExpression>(); node != nullptr) {
            return arena.create<HIRIdentifierExpression>(node->span, node->name, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<BinaryExpression>(); node != nullptr) {
            auto* hir_left = lower_expression(*node->left);
            auto* hir_right = lower_expression(*node->right);

            auto op = static_cast<HIRBinaryExpression::Operator::Type>(static_cast<std::uint8_t>(node->op));

            return arena.create<HIRBinaryExpression>(node->span, hir_left, op, hir_right, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<UnaryExpression>(); node != nullptr) {
            auto* hir_operand = lower_expression(*node->operand);

            auto op = static_cast<HIRUnaryExpression::Operator::Type>(static_cast<std::uint8_t>(node->op));

            return arena.create<HIRUnaryExpression>(node->span, op, hir_operand, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<CallExpression>(); node != nullptr) {
            return lower_call_expression(*node);
        }

        if (auto* node = expression.as<IndexExpression>(); node != nullptr) {
            auto* hir_object = lower_expression(*node->value);
            auto* hir_index = lower_expression(*node->index);

            return arena.create<HIRIndexExpression>(node->span, hir_object, hir_index, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<MemberExpression>(); node != nullptr) {
            auto* hir_object = lower_expression(*node->value);

            return arena.create<HIRMemberExpression>(node->span, hir_object, node->member, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<AssignExpression>(); node != nullptr) {
            auto* hir_target = lower_expression(*node->target);
            auto* hir_value = lower_expression(*node->value);

            return arena.create<HIRAssignExpression>(node->span, hir_target, hir_value, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<StructLiteralExpression>(); node != nullptr) {
            auto hir_fields = std::vector<HIRStructLiteralField>();

            for (auto& field : node->fields) {
                hir_fields.emplace_back(field->name, lower_expression(*field->value));
            }

            auto struct_name = node->name->name;

            if (!node->generic_arguments.empty()) {
                auto generic_args = std::vector<const Type*>();

                for (const auto& argument : node->generic_arguments) {
                    generic_args.push_back(resolver.resolve_type(argument->type->type));
                }

                struct_name = NameMangler::mangle(struct_name, generic_args);
            }

            return arena.create<HIRStructLiteralExpression>(node->span, std::move(struct_name), std::move(hir_fields), resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<IfExpression>(); node != nullptr) {
            auto* hir_condition = lower_expression(*node->condition);
            auto* hir_then = lower_statement(*node->then_branch);

            HIRStatement* hir_else = nullptr;

            if (node->else_branch != nullptr) {
                hir_else = lower_statement(*node->else_branch);
            }

            return arena.create<HIRIfExpression>(node->span, hir_condition, hir_then, hir_else, resolver.resolve_type(node->type));
        }

        if (auto* node = expression.as<TypeExpression>(); node != nullptr) {
            return arena.create<HIRTypeExpression>(node->span, resolver.resolve_type(node->type));
        }

        return nullptr;
    }

    HIRExpression* lower_call_expression(CallExpression& expression) {
        const auto* callee_type = resolver.resolve_type(expression.callee->type);
        const auto* function_type = (callee_type != nullptr) ? callee_type->as<FunctionType>() : nullptr;

        auto hir_arguments = std::vector<HIRExpression*>();

        for (auto& argument : expression.arguments) {
            hir_arguments.push_back(lower_expression(*argument->value));
        }

        HIRExpression* hir_callee = nullptr;

        if (auto* identifier = expression.callee->as<IdentifierExpression>(); identifier != nullptr) {
            const auto* resolved_callee_type = callee_type;
            auto callee_name = std::string();

            if (function_type != nullptr && !expression.generic_arguments.empty() &&
                mono_cache.is_generic_function(identifier->name)) {
                callee_name = lower_monomorphized_call(expression, *identifier, resolved_callee_type);
            } else if (function_type != nullptr) {
                callee_name = lower_ordinary_call_callee(*identifier, function_type);
            } else {
                callee_name = identifier->name;
            }

            hir_callee = arena.create<HIRIdentifierExpression>(identifier->span, std::move(callee_name), resolved_callee_type);
        } else {
            hir_callee = lower_expression(*expression.callee);
        }

        return arena.create<HIRCallExpression>(expression.span, hir_callee, std::move(hir_arguments), resolver.resolve_type(expression.type));
    }

    HIRStatement* lower_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>(); node != nullptr) {
            auto hir_statements = std::vector<HIRStatement*>();

            for (auto& statement_node : node->statements) {
                hir_statements.push_back(lower_statement(*statement_node));
            }

            return arena.create<HIRBlockStatement>(node->span, std::move(hir_statements));
        }

        if (auto* node = statement.as<ExpressionStatement>(); node != nullptr) {
            return arena.create<HIRExpressionStatement>(node->span, lower_expression(*node->expression));
        }

        if (auto* node = statement.as<ReturnStatement>(); node != nullptr) {
            HIRExpression* hir_value = nullptr;

            if (node->value != nullptr) {
                hir_value = lower_expression(*node->value);
            }

            return arena.create<HIRReturnStatement>(node->span, hir_value, resolver.resolve_type(node->type));
        }

        if (auto* node = statement.as<VarDeclaration>(); node != nullptr) {
            HIRExpression* hir_initializer = nullptr;

            if (node->initializer != nullptr) {
                hir_initializer = lower_expression(*node->initializer);
            }

            return arena.create<HIRVarDeclaration>(node->span, node->visibility, node->storage_kind, node->name, resolver.resolve_type(node->type), hir_initializer);
        }

        return nullptr;
    }

    HIRFunctionDeclaration* lower_monomorphized_function(FunctionDeclaration& declaration, 
                                                          const std::string& mangled_name,
                                                          const std::vector<const Type*>& generic_arguments = {}) {
        auto scope = resolver.scoped_substitutions();

        for (std::size_t i = 0; i < declaration.prototype->generic_parameters.size(); ++i) {
            if (i < generic_arguments.size()) {
                resolver.add_substitution(declaration.prototype->generic_parameters[i]->name, generic_arguments[i]);
            }
        }

        auto hir_parameters = std::vector<HIRParameter>();

        for (auto& parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name, resolver.resolve_type(parameter->type->type));
        }

        const auto* hir_return = resolver.resolve_type(declaration.prototype->return_type->type);
        
        auto* hir_statement = lower_statement(*declaration.body);
        auto* hir_body = static_cast<HIRBlockStatement*>(hir_statement);

        bool is_variadic = false;
        if (!declaration.prototype->parameters.empty()) {
            is_variadic = declaration.prototype->parameters.back()->is_variadic;
        }

        return arena.create<HIRFunctionDeclaration>(
            declaration.span, 
            declaration.visibility, 
            mangled_name, 
            std::move(hir_parameters), 
            hir_return, 
            hir_body, 
            is_variadic
        );
    }

  public:
    explicit HIRBuilder(HIRArena& arena, TypeArena& types, Env& env)
        : arena(arena), resolver(types, env), mono_cache() {}

    HIRProgram lower(Program& program) {
        auto hir_program = HIRProgram();

        for (auto& statement : program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                if (!function->prototype->generic_parameters.empty()) {
                    mono_cache.register_function(function->prototype->name, function);
                }
            } else if (auto* struct_declaration = statement->as<StructDeclaration>(); struct_declaration != nullptr) {
                if (!struct_declaration->generic_parameters.empty()) {
                    mono_cache.register_struct(struct_declaration->name, struct_declaration);
                }
            }
        }

        for (auto& statement : program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                if (function->prototype->generic_parameters.empty()) {
                    auto parameter_types = std::vector<const Type*>();

                    for (const auto& parameter : function->prototype->parameters) {
                        parameter_types.push_back(resolver.resolve_type(parameter->type->type));
                    }

                    auto mangled_name = NameMangler::mangle(function->prototype->name, parameter_types);
                    hir_program.functions.push_back(lower_monomorphized_function(*function, mangled_name));
                }
            }
        }

        mono_cache.drain_pending_specializations_into(hir_program.functions);

        return hir_program;
    }
};
