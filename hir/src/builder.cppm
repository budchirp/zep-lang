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
import zep.hir.ir;
import zep.hir.monomorphizer;

export class HIRBuilder {
  private:
    TypeResolver resolver;
    MonomorphizationCache mono_cache;

    const Type* resolve_type(const Type* type) {
        return resolver.resolve_type(type);
    }

    bool check_variadic(const FunctionPrototype& prototype) {
        if (prototype.parameters.empty()) return false;
        return prototype.parameters.back()->is_variadic;
    }

    std::string lower_monomorphized_call(CallExpression& expression,
                                         IdentifierExpression& identifier,
                                         const Type*& callee_type_out) {
        std::vector<const Type*> generic_arguments;
        generic_arguments.reserve(expression.generic_arguments.size());
        for (const auto& generic_argument : expression.generic_arguments) {
            generic_arguments.push_back(resolve_type(generic_argument->type->type));
        }

        MonoCacheResult cache_result = mono_cache.get_or_create(identifier.name, generic_arguments);
        
        if (!cache_result.is_generated) {
            const auto* declaration = mono_cache.get_function(identifier.name);
            if (declaration != nullptr) {
                auto specialization = lower_monomorphized_function(
                    const_cast<FunctionDeclaration&>(*declaration), 
                    cache_result.name
                );
                mono_cache.enqueue_specialization(std::move(specialization));
            }
        }

        callee_type_out = resolve_type(expression.callee->type);
        return cache_result.name;
    }

    std::string lower_ordinary_call_callee(IdentifierExpression& identifier,
                                           const FunctionType* function_type) {
        std::vector<const Type*> parameter_types;
        parameter_types.reserve(function_type->parameters.size());
        for (const auto& param : function_type->parameters) {
            parameter_types.push_back(resolve_type(param.type));
        }
        return NameMangler::mangle(identifier.name, parameter_types);
    }

    std::unique_ptr<HIRExpression> lower_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>()) {
            return std::make_unique<HIRNumberLiteral>(node->span, node->value, resolve_type(node->type));
        }
        if (auto* node = expression.as<FloatLiteral>()) {
            return std::make_unique<HIRFloatLiteral>(node->span, node->value, resolve_type(node->type));
        }
        if (auto* node = expression.as<StringLiteral>()) {
            return std::make_unique<HIRStringLiteral>(node->span, node->value, resolve_type(node->type));
        }
        if (auto* node = expression.as<BooleanLiteral>()) {
            return std::make_unique<HIRBooleanLiteral>(node->span, node->value, resolve_type(node->type));
        }

        if (auto* node = expression.as<IdentifierExpression>()) {
            return std::make_unique<HIRIdentifierExpression>(node->span, node->name, resolve_type(node->type));
        }

        if (auto* node = expression.as<BinaryExpression>()) {
            auto hir_left = lower_expression(*node->left);
            auto hir_right = lower_expression(*node->right);
            auto op = static_cast<HIRBinaryExpression::Operator::Type>(static_cast<std::uint8_t>(node->op));
            return std::make_unique<HIRBinaryExpression>(node->span, std::move(hir_left), op, std::move(hir_right), resolve_type(node->type));
        }

        if (auto* node = expression.as<UnaryExpression>()) {
            auto hir_operand = lower_expression(*node->operand);
            auto op = static_cast<HIRUnaryExpression::Operator::Type>(static_cast<std::uint8_t>(node->op));
            return std::make_unique<HIRUnaryExpression>(node->span, op, std::move(hir_operand), resolve_type(node->type));
        }

        if (auto* node = expression.as<CallExpression>()) {
            return lower_call_expression(*node);
        }

        if (auto* node = expression.as<IndexExpression>()) {
            auto hir_object = lower_expression(*node->value);
            auto hir_index = lower_expression(*node->index);
            return std::make_unique<HIRIndexExpression>(node->span, std::move(hir_object), std::move(hir_index), resolve_type(node->type));
        }

        if (auto* node = expression.as<MemberExpression>()) {
            auto hir_object = lower_expression(*node->value);
            return std::make_unique<HIRMemberExpression>(node->span, std::move(hir_object), node->member, resolve_type(node->type));
        }

        if (auto* node = expression.as<AssignExpression>()) {
            auto hir_target = lower_expression(*node->target);
            auto hir_value = lower_expression(*node->value);
            return std::make_unique<HIRAssignExpression>(node->span, std::move(hir_target), std::move(hir_value), resolve_type(node->type));
        }

        if (auto* node = expression.as<StructLiteralExpression>()) {
            std::vector<HIRStructLiteralField> hir_fields;
            for (auto& field : node->fields) {
                hir_fields.emplace_back(field->name, lower_expression(*field->value));
            }
            std::string struct_name = node->name->name;
            if (!node->generic_arguments.empty()) {
                std::vector<const Type*> generic_args;
                for (const auto& arg : node->generic_arguments) {
                    generic_args.push_back(resolve_type(arg->type->type));
                }
                struct_name = NameMangler::mangle(struct_name, generic_args);
            }
            return std::make_unique<HIRStructLiteralExpression>(node->span, std::move(struct_name), std::move(hir_fields), resolve_type(node->type));
        }

        if (auto* node = expression.as<IfExpression>()) {
            auto hir_condition = lower_expression(*node->condition);
            auto hir_then = lower_statement(*node->then_branch);
            std::unique_ptr<HIRStatement> hir_else;
            if (node->else_branch != nullptr) {
                hir_else = lower_statement(*node->else_branch);
            }
            return std::make_unique<HIRIfExpression>(node->span, std::move(hir_condition), std::move(hir_then), std::move(hir_else), resolve_type(node->type));
        }

        return nullptr;
    }

    std::unique_ptr<HIRExpression> lower_call_expression(CallExpression& expression) {
        const auto* callee_type = resolve_type(expression.callee->type);
        const auto* function_type = (callee_type != nullptr) ? callee_type->as<FunctionType>() : nullptr;

        std::vector<std::unique_ptr<HIRExpression>> hir_arguments;
        for (auto& argument : expression.arguments) {
            hir_arguments.push_back(lower_expression(*argument->value));
        }

        std::unique_ptr<HIRExpression> hir_callee;
        if (auto* identifier = expression.callee->as<IdentifierExpression>()) {
            const Type* resolved_callee_type = callee_type;
            std::string callee_name;

            if (function_type != nullptr && !expression.generic_arguments.empty() &&
                mono_cache.is_generic_function(identifier->name)) {
                callee_name = lower_monomorphized_call(expression, *identifier, resolved_callee_type);
            } else if (function_type != nullptr) {
                callee_name = lower_ordinary_call_callee(*identifier, function_type);
            } else {
                callee_name = identifier->name;
            }

            hir_callee = std::make_unique<HIRIdentifierExpression>(identifier->span, std::move(callee_name), resolved_callee_type);
        } else {
            hir_callee = lower_expression(*expression.callee);
        }

        return std::make_unique<HIRCallExpression>(expression.span, std::move(hir_callee), std::move(hir_arguments), resolve_type(expression.type));
    }

    std::unique_ptr<HIRStatement> lower_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>()) {
            std::vector<std::unique_ptr<HIRStatement>> hir_statements;
            for (auto& stmt : node->statements) {
                hir_statements.push_back(lower_statement(*stmt));
            }
            return std::make_unique<HIRBlockStatement>(node->span, std::move(hir_statements));
        }

        if (auto* node = statement.as<ExpressionStatement>()) {
            return std::make_unique<HIRExpressionStatement>(node->span, lower_expression(*node->expression));
        }

        if (auto* node = statement.as<ReturnStatement>()) {
            std::unique_ptr<HIRExpression> hir_value;
            if (node->value) {
                hir_value = lower_expression(*node->value);
            }
            return std::make_unique<HIRReturnStatement>(node->span, std::move(hir_value), resolve_type(node->type));
        }

        if (auto* node = statement.as<VarDeclaration>()) {
            std::unique_ptr<HIRExpression> hir_initializer;
            if (node->initializer) {
                hir_initializer = lower_expression(*node->initializer);
            }
            return std::make_unique<HIRVarDeclaration>(node->span, node->visibility, node->storage_kind, node->name, resolve_type(node->type), std::move(hir_initializer));
        }

        return nullptr;
    }

    std::unique_ptr<HIRFunctionDeclaration> lower_monomorphized_function(FunctionDeclaration& declaration, 
                                                                         const std::string& mangled_name) {
        std::vector<HIRParameter> hir_parameters;
        for (auto& parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name, resolve_type(parameter->type->type));
        }

        const auto* hir_return = resolve_type(declaration.prototype->return_type->type);
        
        auto hir_stmt = lower_statement(*declaration.body);
        auto* raw_block = static_cast<HIRBlockStatement*>(hir_stmt.release());
        auto hir_body = std::unique_ptr<HIRBlockStatement>(raw_block);

        return std::make_unique<HIRFunctionDeclaration>(
            declaration.span, 
            declaration.visibility, 
            mangled_name, 
            std::move(hir_parameters), 
            hir_return, 
            std::move(hir_body), 
            check_variadic(*declaration.prototype)
        );
    }

  public:
    explicit HIRBuilder(Context& context)
        : resolver(context.types, context.env), mono_cache() {}

    HIRProgram lower(Program& program) {
        std::vector<std::unique_ptr<HIRFunctionDeclaration>> functions;

        for (auto& statement : program.statements) {
            if (auto* func = statement->as<FunctionDeclaration>()) {
                if (!func->prototype->generic_parameters.empty()) {
                    mono_cache.register_function(func->prototype->name, func);
                }
            } else if (auto* struct_decl = statement->as<StructDeclaration>()) {
                if (!struct_decl->generic_parameters.empty()) {
                    mono_cache.register_struct(struct_decl->name, struct_decl);
                }
            }
        }

        for (auto& statement : program.statements) {
            if (auto* func = statement->as<FunctionDeclaration>()) {
                if (func->prototype->generic_parameters.empty()) {
                    std::vector<const Type*> param_types;
                    for (const auto& p : func->prototype->parameters) {
                        param_types.push_back(resolve_type(p->type->type));
                    }
                    std::string mangled_name = NameMangler::mangle(func->prototype->name, param_types);
                    functions.push_back(lower_monomorphized_function(*func, mangled_name));
                }
            }
        }

        mono_cache.drain_pending_specializations_into(functions);

        return HIRProgram(std::move(functions));
    }
};
