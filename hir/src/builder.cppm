module;

#include <memory>
#include <string>
#include <unordered_map>
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
import zep.frontend.sema.context;
import zep.frontend.sema.mangler;
import zep.common.logger;
import zep.frontend.sema.kind;
import zep.frontend.sema.context;
import zep.common.context;
import zep.hir.node;
import zep.hir.context;
import zep.hir.node.program;
import zep.hir.monomorphizer;

export class HIRBuilder : public Visitor<HIRNode*> {
  private:
    HIRProgram program;
    TypeResolver resolver;
    MonomorphizationCache mono_cache;
    SemaContext& sema;

    HIRExpression* lower_expression(Expression& expression) {
        return static_cast<HIRExpression*>(visit_expression(expression));
    }

    HIRStatement* lower_statement(Statement& statement) {
        return static_cast<HIRStatement*>(visit_statement(statement));
    }

    const Type* lower_type(const Type* type, Span span) {
        if (type == nullptr) {
            return nullptr;
        }

        if (auto* pointer = const_cast<Type*>(type)->as<PointerType>(); pointer != nullptr) {
            return sema.types.create<PointerType>(lower_type(pointer->element, span),
                                                  pointer->is_mutable);
        }

        if (auto* array = const_cast<Type*>(type)->as<ArrayType>(); array != nullptr) {
            return sema.types.create<ArrayType>(lower_type(array->element, span), array->size);
        }

        if (auto* function = const_cast<Type*>(type)->as<FunctionType>(); function != nullptr) {
            auto parameters = std::vector<ParameterType>();
            for (const auto& param : function->parameters) {
                parameters.emplace_back(param.name, lower_type(param.type, span));
            }
            return sema.types.create<FunctionType>(
                function->name, lower_type(function->return_type, span), std::move(parameters),
                function->generic_parameters, function->variadic);
        }

        auto* resolved = resolver.resolve(type);

        if (auto* st = const_cast<Type*>(resolved)->as<StructType>(); st != nullptr) {
            std::vector<const Type*> generic_args;
            bool is_specialization = false;

            if (auto* named = const_cast<Type*>(type)->as<NamedType>();
                named != nullptr && !named->generic_arguments.empty()) {
                is_specialization = true;
                for (const auto& arg : named->generic_arguments) {
                    generic_args.push_back(lower_type(arg.type, span));
                }
            } else if (auto* original = mono_cache.get_struct(st->name)) {
                is_specialization = true;
                for (const auto& param : original->generic_parameters) {
                    generic_args.push_back(resolver.resolve(
                        sema.types.create<NamedType>(param->name, std::vector<GenericArgumentType>())));
                }
            }

            if (is_specialization && !generic_args.empty()) {
                auto result = mono_cache.get_or_create(st->name, generic_args);
                if (auto* existing = program.global_scope->lookup_type(result.name)) {
                    return existing->type;
                }

                auto* mangled_st = sema.types.create<StructType>(
                    result.name, std::vector<GenericParameterType>(), std::vector<StructFieldType>());
                register_type_symbol(result.name, Visibility::Type::Public, mangled_st, span);

                for (const auto& field : st->fields) {
                    mangled_st->fields.emplace_back(field.name, lower_type(field.type, span));
                }

                return mangled_st;
            }

            register_type_symbol(st->name, Visibility::Type::Public, st, span);
        }

        return resolved;
    }

    HIRNode* visit(TypeExpression& node) override {
        return program.context.arena.create<HIRTypeExpression>(node.span,
                                                               lower_type(node.type, node.span));
    }

    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(StructField& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(NumberLiteral& node) override {
        return program.context.arena.create<HIRNumberLiteral>(node.span, node.value,
                                                               lower_type(node.type, node.span));
    }

    HIRNode* visit(FloatLiteral& node) override {
        return program.context.arena.create<HIRFloatLiteral>(node.span, node.value,
                                                              lower_type(node.type, node.span));
    }

    HIRNode* visit(StringLiteral& node) override {
        return program.context.arena.create<HIRStringLiteral>(node.span, node.value,
                                                               lower_type(node.type, node.span));
    }

    HIRNode* visit(BooleanLiteral& node) override {
        return program.context.arena.create<HIRBooleanLiteral>(node.span, node.value,
                                                                lower_type(node.type, node.span));
    }

    HIRNode* visit(IdentifierExpression& node) override {
        auto* type = lower_type(node.type, node.span);
        auto name = node.name;

        if (auto* ft = const_cast<Type*>(type)->as<FunctionType>(); ft != nullptr) {
            auto* symbol = program.global_scope->lookup_function(name);

            if (symbol == nullptr || symbol->linkage != Linkage::Type::External) {
                name = ft->name;
            }
        }

        return program.context.arena.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BinaryExpression& node) override {
        auto hir_op = HIRBinaryExpression::Operator::Type::Plus;

        switch (node.op) {
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
            node.span, lower_expression(*node.left), hir_op, lower_expression(*node.right),
            lower_type(node.type, node.span));
    }

    HIRNode* visit(UnaryExpression& node) override {
        auto hir_op = HIRUnaryExpression::Operator::Type::Plus;

        switch (node.op) {
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
            node.span, hir_op, lower_expression(*node.operand), lower_type(node.type, node.span));
    }

    HIRNode* visit(CallExpression& node) override {
        auto hir_arguments = std::vector<HIRExpression*>();

        for (auto* argument : node.arguments) {
            hir_arguments.push_back(lower_expression(*argument->value));
        }

        if (auto* identifier = node.callee->as<IdentifierExpression>(); identifier != nullptr) {
            if (!node.generic_arguments.empty()) {
                auto generic_args = std::vector<const Type*>();

                for (const auto& argument : node.generic_arguments) {
                    generic_args.push_back(lower_type(argument->type->type, node.span));
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

                    for (std::size_t i = 0; i < original->prototype->generic_parameters.size(); ++i) {
                        if (i < generic_args.size()) {
                            resolver.add_substitution(
                                original->prototype->generic_parameters[i]->name, generic_args[i]);
                        }
                    }

                    concrete_callee_type = lower_type(original->type, node.span);
                }

                auto* hir_callee = program.context.arena.create<HIRIdentifierExpression>(
                    identifier->span, result.name, concrete_callee_type);
                return program.context.arena.create<HIRCallExpression>(
                    node.span, hir_callee, std::move(hir_arguments), lower_type(node.type, node.span));
            } else if (auto* function_type = node.callee->type->as<FunctionType>();
                       function_type != nullptr) {
                auto* type = lower_type(node.callee->type, node.span);
                auto name = identifier->name;

                if (auto* ft = const_cast<Type*>(type)->as<FunctionType>(); ft != nullptr) {
                    auto* symbol = program.global_scope->lookup_function(name);

                    if (symbol == nullptr || symbol->linkage != Linkage::Type::External) {
                        name = ft->name;
                    }
                }

                auto* hir_callee = program.context.arena.create<HIRIdentifierExpression>(
                    identifier->span, name, type);

                return program.context.arena.create<HIRCallExpression>(
                    node.span, hir_callee, std::move(hir_arguments), lower_type(node.type, node.span));
            }
        }

        return program.context.arena.create<HIRCallExpression>(
            node.span, lower_expression(*node.callee), std::move(hir_arguments),
            lower_type(node.type, node.span));
    }

    HIRNode* visit(IndexExpression& node) override {
        return program.context.arena.create<HIRIndexExpression>(
            node.span, lower_expression(*node.value), lower_expression(*node.index),
            lower_type(node.type, node.span));
    }

    HIRNode* visit(MemberExpression& node) override {
        return program.context.arena.create<HIRMemberExpression>(
            node.span, lower_expression(*node.value), node.member, lower_type(node.type, node.span));
    }

    HIRNode* visit(AssignExpression& node) override {
        return program.context.arena.create<HIRAssignExpression>(
            node.span, lower_expression(*node.target), lower_expression(*node.value),
            lower_type(node.type, node.span));
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        auto hir_fields = std::vector<HIRStructLiteralField>();
        hir_fields.reserve(node.fields.size());

        for (auto& field : node.fields) {
            hir_fields.emplace_back(field->name, lower_expression(*field->value));
        }

        auto* struct_type = lower_type(node.type, node.span)->as<StructType>();
        auto name = struct_type->name;

        return program.context.arena.create<HIRStructLiteralExpression>(
            node.span, name, std::move(hir_fields), struct_type);
    }

    HIRNode* visit(IfExpression& node) override {
        HIRStatement* hir_else = nullptr;

        if (node.else_branch != nullptr) {
            hir_else = lower_statement(*node.else_branch);
        }

        return program.context.arena.create<HIRIfExpression>(
            node.span, lower_expression(*node.condition), lower_statement(*node.then_branch),
            hir_else, lower_type(node.type, node.span));
    }

    HIRNode* visit(BlockStatement& node) override {
        auto hir_statements = std::vector<HIRStatement*>();

        for (auto& child : node.statements) {
            auto* hir_statement = lower_statement(*child);
            if (hir_statement != nullptr) {
                hir_statements.push_back(hir_statement);
            }
        }

        return program.context.arena.create<HIRBlockStatement>(node.span,
                                                               std::move(hir_statements));
    }

    HIRNode* visit(ExpressionStatement& node) override {
        return program.context.arena.create<HIRExpressionStatement>(node.span,
                                                                    lower_expression(*node.expression));
    }

    HIRNode* visit(ReturnStatement& node) override {
        HIRExpression* hir_value = nullptr;

        if (node.value != nullptr) {
            hir_value = lower_expression(*node.value);
        }

        return program.context.arena.create<HIRReturnStatement>(node.span, hir_value,
                                                                lower_type(node.type, node.span));
    }

    HIRNode* visit(VarDeclaration& node) override {
        HIRExpression* hir_initializer = nullptr;

        if (node.initializer != nullptr) {
            hir_initializer = lower_expression(*node.initializer);
        }

        const Type* var_type = node.type;
        if (node.annotation != nullptr) {
            var_type = lower_type(node.annotation->type, node.span);
        } else {
            var_type = lower_type(node.type, node.span);
        }

        return program.context.arena.create<HIRVarDeclaration>(
            node.span, node.visibility, node.storage_kind, node.name, var_type, hir_initializer);
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (node.prototype->generic_parameters.empty()) {
            auto* mangled_type = lower_type(node.type, node.span)->as<FunctionType>();
            auto mangled_name = mangled_type->name;

            register_function_symbol(mangled_name, node.visibility, Linkage::Type::Internal,
                                     mangled_type, node.span);

            return lower_monomorphized_function(node, mangled_name);
        }

        return nullptr;
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        auto hir_parameters = std::vector<HIRParameter>();

        for (const auto& parameter : node.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name, lower_type(parameter->type->type, node.span));
        }

        auto is_variadic =
            !node.prototype->parameters.empty() && node.prototype->parameters.back()->is_variadic;

        auto* type = lower_type(node.type, node.span);

        register_function_symbol(node.prototype->name, node.visibility, Linkage::Type::External,
                                 type, node.span);

        return program.context.arena.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, node.prototype->name,
            std::move(hir_parameters), lower_type(node.prototype->return_type->type, node.span), nullptr,
            is_variadic, type);
    }

    HIRNode* visit(StructDeclaration& node) override {
        if (!node.generic_parameters.empty()) {
            mono_cache.register_struct(node.name, &node);
        }

        return nullptr;
    }

    HIRNode* visit(ExternVarDeclaration& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(ImportStatement& node [[maybe_unused]]) override { return nullptr; }

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
        hir_parameters.reserve(declaration.prototype->parameters.size());

        for (const auto& parameter : declaration.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name, lower_type(parameter->type->type, declaration.span));
        }

        auto is_variadic = !declaration.prototype->parameters.empty() &&
                           declaration.prototype->parameters.back()->is_variadic;

        const auto* return_type = lower_type(declaration.prototype->return_type->type, declaration.span);
        const auto* function_type = lower_type(declaration.type, declaration.span)->as<FunctionType>();

        auto* mangled_type =
            sema.types.create<FunctionType>(mangled_name, return_type, function_type->parameters,
                                            function_type->generic_parameters, is_variadic);

        return program.context.arena.create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::Internal, mangled_name,
            std::move(hir_parameters), return_type,
            static_cast<HIRBlockStatement*>(lower_statement(*declaration.body)), is_variadic,
            mangled_type);
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
    explicit HIRBuilder(SemaContext& sema)
        : program(), resolver(sema.types, sema.env), mono_cache(), sema(sema) {}

    HIRProgram lower(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                if (!function->prototype->generic_parameters.empty()) {
                    mono_cache.register_function(function->prototype->name, function);
                } else {
                    visit_statement(*function);
                }
            } else if (auto* struct_decl = statement->as<StructDeclaration>(); struct_decl != nullptr) {
                visit_statement(*struct_decl);
            } else {
                visit_statement(*statement);
            }
        }

        while (true) {
            auto pending = std::vector<HIRFunctionDeclaration*>();
            mono_cache.drain_pending_specializations_into(pending);

            if (pending.empty()) {
                break;
            }

            for (auto* function : pending) {
                program.statements.push_back(function);
            }
        }

        for (auto* statement : program.statements) {
            if (auto* function = statement->as<HIRFunctionDeclaration>(); function != nullptr) {
                register_function_symbol(function->name, function->visibility, function->linkage,
                                         function->type, function->span);
            }
        }

        return std::move(program);
    }
};
