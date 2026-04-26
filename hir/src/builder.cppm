module;

#include <memory>
#include <string>
#include <vector>

export module zep.hir.builder;

import zep.common.span;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.mangler;
import zep.frontend.sema.kind;
import zep.frontend.sema.context;
import zep.common.context;
import zep.hir.node;
import zep.hir.context;
import zep.hir.node.program;
import zep.hir.monomorphizer;

export class HIRBuilder {
  private:
    HIRProgram program;
    TypeResolver resolver;
    MonomorphizationCache mono_cache;

    HIRExpression* lower_expression(Expression& expression) {
        if (auto* node = expression.as<NumberLiteral>(); node != nullptr) {
            return program.context.arena.create<HIRNumberLiteral>(node->span, node->value,
                                                                  resolver.resolve(node->type));
        }

        if (auto* node = expression.as<FloatLiteral>(); node != nullptr) {
            return program.context.arena.create<HIRFloatLiteral>(node->span, node->value,
                                                                 resolver.resolve(node->type));
        }

        if (auto* node = expression.as<StringLiteral>(); node != nullptr) {
            return program.context.arena.create<HIRStringLiteral>(node->span, node->value,
                                                                  resolver.resolve(node->type));
        }

        if (auto* node = expression.as<BooleanLiteral>(); node != nullptr) {
            return program.context.arena.create<HIRBooleanLiteral>(node->span, node->value,
                                                                   resolver.resolve(node->type));
        }

        if (auto* node = expression.as<IdentifierExpression>(); node != nullptr) {
            return program.context.arena.create<HIRIdentifierExpression>(
                node->span, node->name, resolver.resolve(node->type));
        }

        if (auto* node = expression.as<BinaryExpression>(); node != nullptr) {
            auto hir_op = HIRBinaryExpression::Operator::Type::Plus;

            switch (node->op) {
            case BinaryExpression::Operator::Type::Plus:
                hir_op = HIRBinaryExpression::Operator::Type::Plus;
                break;
            case BinaryExpression::Operator::Type::Minus:
                hir_op = HIRBinaryExpression::Operator::Type::Minus;
                break;
            case BinaryExpression::Operator::Type::Asterisk:
                hir_op = HIRBinaryExpression::Operator::Type::Asterisk;
                break;
            case BinaryExpression::Operator::Type::Divide:
                hir_op = HIRBinaryExpression::Operator::Type::Divide;
                break;
            case BinaryExpression::Operator::Type::Modulo:
                hir_op = HIRBinaryExpression::Operator::Type::Modulo;
                break;
            case BinaryExpression::Operator::Type::Equals:
                hir_op = HIRBinaryExpression::Operator::Type::Equals;
                break;
            case BinaryExpression::Operator::Type::NotEquals:
                hir_op = HIRBinaryExpression::Operator::Type::NotEquals;
                break;
            case BinaryExpression::Operator::Type::LessThan:
                hir_op = HIRBinaryExpression::Operator::Type::LessThan;
                break;
            case BinaryExpression::Operator::Type::GreaterThan:
                hir_op = HIRBinaryExpression::Operator::Type::GreaterThan;
                break;
            case BinaryExpression::Operator::Type::LessEqual:
                hir_op = HIRBinaryExpression::Operator::Type::LessEqual;
                break;
            case BinaryExpression::Operator::Type::GreaterEqual:
                hir_op = HIRBinaryExpression::Operator::Type::GreaterEqual;
                break;
            case BinaryExpression::Operator::Type::And:
                hir_op = HIRBinaryExpression::Operator::Type::And;
                break;
            case BinaryExpression::Operator::Type::Or:
                hir_op = HIRBinaryExpression::Operator::Type::Or;
                break;
            case BinaryExpression::Operator::Type::As:
                hir_op = HIRBinaryExpression::Operator::Type::As;
                break;
            case BinaryExpression::Operator::Type::Is:
                hir_op = HIRBinaryExpression::Operator::Type::Is;
                break;
            }

            return program.context.arena.create<HIRBinaryExpression>(
                node->span, lower_expression(*node->left), hir_op, lower_expression(*node->right),
                resolver.resolve(node->type));
        }

        if (auto* node = expression.as<UnaryExpression>(); node != nullptr) {
            auto hir_op = HIRUnaryExpression::Operator::Type::Plus;

            switch (node->op) {
            case UnaryExpression::Operator::Type::Plus:
                hir_op = HIRUnaryExpression::Operator::Type::Plus;
                break;
            case UnaryExpression::Operator::Type::Minus:
                hir_op = HIRUnaryExpression::Operator::Type::Minus;
                break;
            case UnaryExpression::Operator::Type::Not:
                hir_op = HIRUnaryExpression::Operator::Type::Not;
                break;
            case UnaryExpression::Operator::Type::Dereference:
                hir_op = HIRUnaryExpression::Operator::Type::Dereference;
                break;
            case UnaryExpression::Operator::Type::AddressOf:
                hir_op = HIRUnaryExpression::Operator::Type::AddressOf;
                break;
            }

            return program.context.arena.create<HIRUnaryExpression>(
                node->span, hir_op, lower_expression(*node->operand), resolver.resolve(node->type));
        }

        if (auto* node = expression.as<CallExpression>(); node != nullptr) {
            auto hir_arguments = std::vector<HIRExpression*>();

            for (auto* argument : node->arguments) {
                hir_arguments.push_back(lower_expression(*argument->value));
            }

            if (auto* identifier = node->callee->as<IdentifierExpression>();
                identifier != nullptr) {
                if (!node->generic_arguments.empty()) {
                    auto generic_args = std::vector<const Type*>();

                    for (const auto& argument : node->generic_arguments) {
                        generic_args.push_back(resolver.resolve(argument->type->type));
                    }

                    auto result = mono_cache.get_or_create(identifier->name, generic_args);

                    auto* original = mono_cache.get_function(identifier->name);

                    if (!result.is_generated && original != nullptr) {
                        mono_cache.enqueue_specialization(
                            lower_monomorphized_function(*original, result.name, generic_args));
                    }

                    const Type* concrete_callee_type = nullptr;

                    if (original != nullptr) {
                        auto sub_scope = resolver.scoped_substitutions();

                        for (std::size_t i = 0; i < original->prototype->generic_parameters.size();
                             ++i) {
                            if (i < generic_args.size()) {
                                resolver.add_substitution(
                                    original->prototype->generic_parameters[i]->name,
                                    generic_args[i]);
                            }
                        }

                        concrete_callee_type = resolver.resolve(original->type);
                    }

                    auto* hir_callee = program.context.arena.create<HIRIdentifierExpression>(
                        identifier->span, result.name, concrete_callee_type);
                    return program.context.arena.create<HIRCallExpression>(
                        node->span, hir_callee, std::move(hir_arguments),
                        resolver.resolve(node->type));
                } else if (auto* function_type = node->callee->type->as<FunctionType>();
                           function_type != nullptr) {
                    auto name = identifier->name;
                    auto parameter_types = std::vector<const Type*>();

                    for (const auto& param : function_type->parameters) {
                        parameter_types.push_back(param.type);
                    }

                    auto* symbol = program.global_scope->lookup_function(name);

                    if (symbol == nullptr || symbol->linkage != Linkage::Type::External) {
                        name = NameMangler::mangle(identifier->name, parameter_types);
                    }

                    auto* hir_callee = program.context.arena.create<HIRIdentifierExpression>(
                        identifier->span, name, resolver.resolve(node->callee->type));

                    return program.context.arena.create<HIRCallExpression>(
                        node->span, hir_callee, std::move(hir_arguments),
                        resolver.resolve(node->type));
                }
            }

            return program.context.arena.create<HIRCallExpression>(
                node->span, lower_expression(*node->callee), std::move(hir_arguments),
                resolver.resolve(node->type));
        }

        if (auto* node = expression.as<IndexExpression>(); node != nullptr) {
            return program.context.arena.create<HIRIndexExpression>(
                node->span, lower_expression(*node->value), lower_expression(*node->index),
                resolver.resolve(node->type));
        }

        if (auto* node = expression.as<MemberExpression>(); node != nullptr) {
            return program.context.arena.create<HIRMemberExpression>(
                node->span, lower_expression(*node->value), node->member,
                resolver.resolve(node->type));
        }

        if (auto* node = expression.as<AssignExpression>(); node != nullptr) {
            return program.context.arena.create<HIRAssignExpression>(
                node->span, lower_expression(*node->target), lower_expression(*node->value),
                resolver.resolve(node->type));
        }

        if (auto* node = expression.as<StructLiteralExpression>(); node != nullptr) {
            auto hir_fields = std::vector<HIRStructLiteralField>();

            for (auto& field : node->fields) {
                hir_fields.emplace_back(field->name, lower_expression(*field->value));
            }

            auto struct_name = node->name->name;
            auto* struct_type = resolver.resolve(node->type);

            if (!node->generic_arguments.empty()) {
                auto generic_args = std::vector<const Type*>();

                for (const auto& argument : node->generic_arguments) {
                    generic_args.push_back(resolver.resolve(argument->type->type));
                }

                struct_name = NameMangler::mangle(struct_name, generic_args);
            }

            register_type_symbol(struct_name, Visibility::Type::Public, struct_type, node->span);

            return program.context.arena.create<HIRStructLiteralExpression>(
                node->span, std::move(struct_name), std::move(hir_fields), struct_type);
        }

        if (auto* node = expression.as<IfExpression>(); node != nullptr) {
            HIRStatement* hir_else = nullptr;

            if (node->else_branch != nullptr) {
                hir_else = lower_statement(*node->else_branch);
            }

            return program.context.arena.create<HIRIfExpression>(
                node->span, lower_expression(*node->condition), lower_statement(*node->then_branch),
                hir_else, resolver.resolve(node->type));
        }

        if (auto* node = expression.as<TypeExpression>(); node != nullptr) {
            return program.context.arena.create<HIRTypeExpression>(node->span,
                                                                   resolver.resolve(node->type));
        }

        return nullptr;
    }

    HIRStatement* lower_statement(Statement& statement) {
        if (auto* node = statement.as<BlockStatement>(); node != nullptr) {
            auto hir_statements = std::vector<HIRStatement*>();

            for (auto& child : node->statements) {
                hir_statements.push_back(lower_statement(*child));
            }

            return program.context.arena.create<HIRBlockStatement>(node->span,
                                                                   std::move(hir_statements));
        }

        if (auto* node = statement.as<ExpressionStatement>(); node != nullptr) {
            return program.context.arena.create<HIRExpressionStatement>(
                node->span, lower_expression(*node->expression));
        }

        if (auto* node = statement.as<ReturnStatement>(); node != nullptr) {
            HIRExpression* hir_value = nullptr;

            if (node->value != nullptr) {
                hir_value = lower_expression(*node->value);
            }

            return program.context.arena.create<HIRReturnStatement>(node->span, hir_value,
                                                                    resolver.resolve(node->type));
        }

        if (auto* node = statement.as<VarDeclaration>(); node != nullptr) {
            HIRExpression* hir_initializer = nullptr;

            if (node->initializer != nullptr) {
                hir_initializer = lower_expression(*node->initializer);
            }

            return program.context.arena.create<HIRVarDeclaration>(
                node->span, node->visibility, node->storage_kind, node->name,
                resolver.resolve(node->type), hir_initializer);
        }


        if (auto* node = statement.as<FunctionDeclaration>(); node != nullptr) {
            if (node->prototype->generic_parameters.empty()) {
                auto parameter_types = std::vector<const Type*>();

                for (const auto& parameter : node->prototype->parameters) {
                    parameter_types.push_back(resolver.resolve(parameter->type->type));
                }

                auto mangled_name = NameMangler::mangle(node->prototype->name, parameter_types);

                register_function_symbol(mangled_name, node->visibility, Linkage::Type::Internal,
                                         resolver.resolve(node->type), node->span);

                return lower_monomorphized_function(*node, mangled_name);
            }
        }

        if (auto* node = statement.as<ExternFunctionDeclaration>(); node != nullptr) {
            auto hir_parameters = std::vector<HIRParameter>();

            for (const auto& parameter : node->prototype->parameters) {
                hir_parameters.emplace_back(parameter->name,
                                            resolver.resolve(parameter->type->type));
            }

            auto is_variadic = !node->prototype->parameters.empty() &&
                               node->prototype->parameters.back()->is_variadic;

            register_function_symbol(node->prototype->name, node->visibility, Linkage::Type::External,
                                     resolver.resolve(node->type), node->span);

            return program.context.arena.create<HIRFunctionDeclaration>(
                node->span, node->visibility, Linkage::Type::External, node->prototype->name,
                std::move(hir_parameters), resolver.resolve(node->prototype->return_type->type),
                nullptr, is_variadic, resolver.resolve(node->type));
        }

        return nullptr;
    }


    HIRFunctionDeclaration*
    lower_monomorphized_function(const FunctionDeclaration& declaration,
                                 const std::string& mangled_name,
                                 const std::vector<const Type*>& generic_arguments = {}) {
        auto sub_scope = resolver.scoped_substitutions();

        for (std::size_t i = 0; i < declaration.prototype->generic_parameters.size(); ++i) {
            if (i < generic_arguments.size()) {
                resolver.add_substitution(declaration.prototype->generic_parameters[i]->name,
                                          generic_arguments[i]);
            }
        }

        auto hir_parameters = std::vector<HIRParameter>();

        for (const auto& parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name, resolver.resolve(parameter->type->type));
        }

        auto is_variadic = !declaration.prototype->parameters.empty() &&
                           declaration.prototype->parameters.back()->is_variadic;

        return program.context.arena.create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::Internal, mangled_name,
            std::move(hir_parameters), resolver.resolve(declaration.prototype->return_type->type),
            static_cast<HIRBlockStatement*>(
                lower_statement(*const_cast<BlockStatement*>(declaration.body))),
            is_variadic, resolver.resolve(declaration.type));
    }

    void register_function_symbol(const std::string& name, Visibility::Type visibility,
                                  Linkage::Type linkage, const Type* type, Span span) {
        if (program.global_scope->lookup_function(name) != nullptr) {
            return;
        }

        auto* symbol = program.context.symbol_arena.create<FunctionSymbol>(name, span, visibility,
                                                                          linkage, type);
        program.global_scope->define_function(name, symbol);
    }

    void register_type_symbol(const std::string& name, Visibility::Type visibility,
                              const Type* type, Span span) {
        if (program.global_scope->lookup_type(name) != nullptr) {
            return;
        }

        auto* symbol =
            program.context.symbol_arena.create<TypeSymbol>(name, span, visibility, type);
        program.global_scope->define_type(name, symbol);
    }

  public:
    explicit HIRBuilder(Context& context, SemaContext& sema)
        : program(), resolver(sema.types, sema.env), mono_cache() {}

    HIRProgram lower(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                if (!function->prototype->generic_parameters.empty()) {
                    mono_cache.register_function(function->prototype->name, function);
                }
            } else if (auto* struct_declaration = statement->as<StructDeclaration>();
                       struct_declaration != nullptr) {
                if (!struct_declaration->generic_parameters.empty()) {
                    mono_cache.register_struct(struct_declaration->name, struct_declaration);
                }
            }
        }

        for (auto& statement : ast_program.statements) {
            auto* hir_statement = lower_statement(*statement);

            if (hir_statement != nullptr) {
                program.statements.push_back(hir_statement);

                if (auto* hir_function = hir_statement->as<HIRFunctionDeclaration>();
                    hir_function != nullptr) {
                    program.functions.push_back(hir_function);
                }
            }
        }

        auto pending = std::vector<HIRFunctionDeclaration*>();
        mono_cache.drain_pending_specializations_into(pending);

        for (auto* function : pending) {
            program.functions.push_back(function);
            program.statements.push_back(function);
        }

        for (auto* function : program.functions) {
            register_function_symbol(function->name, function->visibility, function->linkage,
                                     function->type, function->span);
        }

        return std::move(program);
    }
};
