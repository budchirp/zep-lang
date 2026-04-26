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
import zep.hir.lowerer;

export class HIRBuilder : public Visitor<HIRNode*> {
  public:
    HIRProgram program;
    TypeResolver resolver;
    MonomorphizationCache mono_cache;
    SemaContext& sema;
    HIRLowerer<HIRBuilder> lowerer;

    HIRExpression* lower_expression(Expression& expression) {
        return static_cast<HIRExpression*>(visit_expression(expression));
    }

    HIRStatement* lower_statement(Statement& statement) {
        return static_cast<HIRStatement*>(visit_statement(statement));
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

    void register_function_symbol(const std::string& name, Visibility::Type visibility,
                                  Linkage::Type linkage, const Type* type, Span span) {
        if (program.global_scope->lookup_function(name) != nullptr) {
            return;
        }

        auto* symbol = program.context.symbol_arena.create<FunctionSymbol>(name, span, visibility,
                                                                           linkage, type);
        program.global_scope->define_function(name, symbol);
    }

  private:
    HIRNode* visit(TypeExpression& node) override {
        return program.context.arena.create<HIRTypeExpression>(
            node.span, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(NumberLiteral& node) override {
        return program.context.arena.create<HIRNumberLiteral>(
            node.span, node.value, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(FloatLiteral& node) override {
        return program.context.arena.create<HIRFloatLiteral>(
            node.span, node.value, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(StringLiteral& node) override {
        return program.context.arena.create<HIRStringLiteral>(
            node.span, node.value, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(BooleanLiteral& node) override {
        return program.context.arena.create<HIRBooleanLiteral>(
            node.span, node.value, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(IdentifierExpression& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);
        auto name = node.name;

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            auto* symbol = program.global_scope->lookup_function(name);

            if (symbol == nullptr || symbol->linkage != Linkage::Type::External) {
                name = function_type->name;
            }
        }

        return program.context.arena.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BinaryExpression& node) override {
        return program.context.arena.create<HIRBinaryExpression>(
            node.span, lower_expression(*node.left), node.op, lower_expression(*node.right),
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(UnaryExpression& node) override {
        return program.context.arena.create<HIRUnaryExpression>(
            node.span, node.op, lower_expression(*node.operand),
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(CallExpression& node) override {
        std::vector<HIRExpression*> hir_arguments;
        hir_arguments.reserve(node.arguments.size());

        for (auto* argument : node.arguments) {
            hir_arguments.push_back(lower_expression(*argument->value));
        }

        if (const auto* function_type = node.callee->type->as<FunctionType>();
            function_type != nullptr && !node.generic_arguments.empty()) {
            auto generic_args = lowerer.lower_generic_arguments(node.generic_arguments, node.span);

            auto result = mono_cache.get_or_create(function_type->name, generic_args);

            auto* original = mono_cache.get_function(function_type->name);

            if (!result.is_generated && original != nullptr) {
                mono_cache.enqueue_specialization(
                    lowerer.lower_monomorphized_function(*original, result.name, generic_args));
            }

            const Type* concrete_callee_type = nullptr;

            if (original != nullptr) {
                auto sub_scope = resolver.scoped_substitutions();
                lowerer.apply_generic_substitutions(original->prototype->generic_parameters,
                                                    generic_args);
                concrete_callee_type = lowerer.lower_type(original->type, node.span);
            }

            auto* hir_callee = program.context.arena.create<HIRIdentifierExpression>(
                node.callee->span, result.name, concrete_callee_type);
            return program.context.arena.create<HIRCallExpression>(
                node.span, hir_callee, std::move(hir_arguments),
                lowerer.lower_type(node.type, node.span));
        }

        return program.context.arena.create<HIRCallExpression>(
            node.span, lower_expression(*node.callee), std::move(hir_arguments),
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(IndexExpression& node) override {
        return program.context.arena.create<HIRIndexExpression>(
            node.span, lower_expression(*node.value), lower_expression(*node.index),
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(MemberExpression& node) override {
        return program.context.arena.create<HIRMemberExpression>(
            node.span, lower_expression(*node.value), node.member,
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(AssignExpression& node) override {
        return program.context.arena.create<HIRAssignExpression>(
            node.span, lower_expression(*node.target), lower_expression(*node.value),
            lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        std::vector<HIRStructLiteralField> hir_fields;
        hir_fields.reserve(node.fields.size());

        for (auto& field : node.fields) {
            hir_fields.emplace_back(field->name, lower_expression(*field->value));
        }

        const auto* struct_type = lowerer.lower_type(node.type, node.span)->as<StructType>();
        auto name = struct_type->name;

        return program.context.arena.create<HIRStructLiteralExpression>(
            node.span, name, std::move(hir_fields), struct_type);
    }

    HIRNode* visit(IfExpression& node) override {
        auto* hir_else = node.else_branch != nullptr ? lower_statement(*node.else_branch) : nullptr;
        return program.context.arena.create<HIRIfExpression>(
            node.span, lower_expression(*node.condition), lower_statement(*node.then_branch),
            hir_else, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(BlockStatement& node) override {
        std::vector<HIRStatement*> hir_statements;
        hir_statements.reserve(node.statements.size());

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
        return program.context.arena.create<HIRExpressionStatement>(
            node.span, lower_expression(*node.expression));
    }

    HIRNode* visit(ReturnStatement& node) override {
        auto* hir_value = node.value != nullptr ? lower_expression(*node.value) : nullptr;
        return program.context.arena.create<HIRReturnStatement>(
            node.span, hir_value, lowerer.lower_type(node.type, node.span));
    }

    HIRNode* visit(VarDeclaration& node) override {
        auto* hir_initializer =
            node.initializer != nullptr ? lower_expression(*node.initializer) : nullptr;
        const auto* var_type = lowerer.lower_type(
            node.annotation != nullptr ? node.annotation->type : node.type, node.span);

        return program.context.arena.create<HIRVarDeclaration>(
            node.span, node.visibility, node.storage_kind, node.name, var_type, hir_initializer);
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (node.prototype->generic_parameters.empty()) {
            const auto* mangled_type = lowerer.lower_type(node.type, node.span)->as<FunctionType>();
            auto mangled_name = mangled_type->name;

            register_function_symbol(mangled_name, node.visibility, Linkage::Type::Internal,
                                     mangled_type, node.span);

            return lowerer.lower_monomorphized_function(node, mangled_name);
        }

        return nullptr;
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        std::vector<HIRParameter> hir_parameters;
        hir_parameters.reserve(node.prototype->parameters.size());

        for (const auto& parameter : node.prototype->parameters) {
            hir_parameters.emplace_back(parameter->name,
                                        lowerer.lower_type(parameter->type->type, node.span));
        }

        auto is_variadic =
            !node.prototype->parameters.empty() && node.prototype->parameters.back()->is_variadic;

        auto* type = lowerer.lower_type(node.type, node.span);

        register_function_symbol(node.prototype->name, node.visibility, Linkage::Type::External,
                                 type, node.span);

        return program.context.arena.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, node.prototype->name,
            std::move(hir_parameters),
            lowerer.lower_type(node.prototype->return_type->type, node.span), nullptr, is_variadic,
            type);
    }

    HIRNode* visit(StructDeclaration& node) override {
        if (!node.generic_parameters.empty()) {
            mono_cache.register_struct(node.name, &node);
        }

        return nullptr;
    }

    HIRNode* visit(ExternVarDeclaration& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(ImportStatement& node [[maybe_unused]]) override { return nullptr; }

  public:
    explicit HIRBuilder(SemaContext& sema)
        : resolver(sema.types, sema.env), mono_cache(), sema(sema),
          lowerer(program, resolver, mono_cache, sema, *this) {}

    HIRProgram lower(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>();
                function != nullptr && !function->prototype->generic_parameters.empty()) {
                mono_cache.register_function(function->prototype->name, function);
            } else {
                auto* lowered = visit_statement(*statement);
                if (lowered != nullptr) {
                    program.statements.push_back(static_cast<HIRStatement*>(lowered));
                }
            }
        }

        std::vector<HIRFunctionDeclaration*> pending;
        while (true) {
            pending.clear();
            mono_cache.drain_pending_specializations_into(pending);

            if (pending.empty()) {
                break;
            }

            program.statements.insert(program.statements.end(), pending.begin(), pending.end());
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
