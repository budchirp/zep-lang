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
import zep.common.context;
import zep.hir.node;
import zep.hir.context;
import zep.hir.node.program;
import zep.hir.monomorphizer;
import zep.hir.lowerer;

export class HIRBuilder : public Visitor<HIRNode*> {
  private:
    template <typename Builder>
    friend class HIRLowerer;

    HIRProgram program;

    SemaContext& sema;

    TypeResolver resolver;

    MonomorphizationCache mono_cache;

    HIRLowerer<HIRBuilder> lowerer;

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

    HIRNode* visit(TypeExpression& node) override {
        const auto* lowered_type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRTypeExpression>(node.span, lowered_type);
    }

    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(NumberLiteral& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRNumberLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(FloatLiteral& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRFloatLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(StringLiteral& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRStringLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(BooleanLiteral& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRBooleanLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(IdentifierExpression& node) override {
        const auto* type = lowerer.lower_type(node.type, node.span);
        auto name = node.name;

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            const auto* symbol = program.global_scope->lookup_function(name);

            if (symbol == nullptr || symbol->linkage != Linkage::Type::External) {
                name = function_type->name;
            }
        }

        return program.context.nodes.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BinaryExpression& node) override {
        auto* left = static_cast<HIRExpression*>(visit_expression(*node.left));
        auto* right = static_cast<HIRExpression*>(visit_expression(*node.right));
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRBinaryExpression>(node.span, left, node.op, right,
                                                                 type);
    }

    HIRNode* visit(UnaryExpression& node) override {
        auto* operand = static_cast<HIRExpression*>(visit_expression(*node.operand));
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRUnaryExpression>(node.span, node.op, operand, type);
    }

    HIRNode* visit(CallExpression& node) override {
        std::vector<HIRExpression*> arguments;
        arguments.reserve(node.arguments.size());

        for (const auto* argument : node.arguments) {
            arguments.push_back(static_cast<HIRExpression*>(visit_expression(*argument->value)));
        }

        const auto* function_type = node.callee->type->as<FunctionType>();

        if (function_type == nullptr || node.generic_arguments.empty()) {
            auto* callee = static_cast<HIRExpression*>(visit_expression(*node.callee));
            const auto* type = lowerer.lower_type(node.type, node.span);

            return program.context.nodes.create<HIRCallExpression>(node.span, callee,
                                                                   std::move(arguments), type);
        }

        const auto argument_types =
            lowerer.lower_generic_arguments(node.generic_arguments, node.span);
        const auto instance = mono_cache.get_or_create(function_type->name, argument_types);
        const auto* definition = mono_cache.get_function(function_type->name);

        if (!instance.is_generated && definition != nullptr) {
            mono_cache.enqueue_specialization(
                lowerer.lower_monomorphized_function(*definition, instance.name, argument_types));
        }

        const Type* callee_type = nullptr;

        if (definition != nullptr) {
            auto scope = resolver.scoped_substitutions();

            lowerer.apply_generic_substitutions(definition->prototype->generic_parameters,
                                                argument_types);
            callee_type = lowerer.lower_type(definition->type, node.span);
        }

        auto* callee = program.context.nodes.create<HIRIdentifierExpression>(
            node.callee->span, instance.name, callee_type);
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRCallExpression>(node.span, callee,
                                                               std::move(arguments), type);
    }

    HIRNode* visit(IndexExpression& node) override {
        auto* value = static_cast<HIRExpression*>(visit_expression(*node.value));
        auto* index = static_cast<HIRExpression*>(visit_expression(*node.index));
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRIndexExpression>(node.span, value, index, type);
    }

    HIRNode* visit(MemberExpression& node) override {
        auto* value = static_cast<HIRExpression*>(visit_expression(*node.value));
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRMemberExpression>(node.span, value, node.member,
                                                                 type);
    }

    HIRNode* visit(AssignExpression& node) override {
        auto* target = static_cast<HIRExpression*>(visit_expression(*node.target));
        auto* value = static_cast<HIRExpression*>(visit_expression(*node.value));
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRAssignExpression>(node.span, target, value, type);
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        std::vector<HIRStructLiteralField> fields;
        fields.reserve(node.fields.size());

        for (const auto* field : node.fields) {
            fields.emplace_back(field->name, static_cast<HIRExpression*>(visit_expression(*field->value)));
        }

        const auto* struct_type = lowerer.lower_type(node.type, node.span)->as<StructType>();
        const auto& struct_name = struct_type->name;

        return program.context.nodes.create<HIRStructLiteralExpression>(
            node.span, struct_name, std::move(fields), struct_type);
    }

    HIRNode* visit(IfExpression& node) override {
        auto* condition = static_cast<HIRExpression*>(visit_expression(*node.condition));
        auto* then_branch = static_cast<HIRStatement*>(visit_statement(*node.then_branch));
        auto* else_branch = node.else_branch != nullptr
                                ? static_cast<HIRStatement*>(visit_statement(*node.else_branch))
                                : nullptr;
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRIfExpression>(node.span, condition, then_branch,
                                                             else_branch, type);
    }

    HIRNode* visit(BlockStatement& node) override {
        std::vector<HIRStatement*> body;
        body.reserve(node.statements.size());

        for (auto& statement : node.statements) {
            if (auto* lowered = static_cast<HIRStatement*>(visit_statement(*statement));
                lowered != nullptr) {
                body.push_back(lowered);
            }
        }

        return program.context.nodes.create<HIRBlockStatement>(node.span, std::move(body));
    }

    HIRNode* visit(ExpressionStatement& node) override {
        auto* expression = static_cast<HIRExpression*>(visit_expression(*node.expression));

        return program.context.nodes.create<HIRExpressionStatement>(node.span, expression);
    }

    HIRNode* visit(ReturnStatement& node) override {
        auto* value = node.value != nullptr
                          ? static_cast<HIRExpression*>(visit_expression(*node.value))
                          : nullptr;
        const auto* type = lowerer.lower_type(node.type, node.span);

        return program.context.nodes.create<HIRReturnStatement>(node.span, value, type);
    }

    HIRNode* visit(VarDeclaration& node) override {
        auto* initializer = node.initializer != nullptr
                                ? static_cast<HIRExpression*>(visit_expression(*node.initializer))
                                : nullptr;
        const auto* variable_type = lowerer.lower_type(
            node.annotation != nullptr ? node.annotation->type : node.type, node.span);

        return program.context.nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, node.storage_kind, node.name, variable_type, initializer);
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (!node.prototype->generic_parameters.empty()) {
            return nullptr;
        }

        const auto* signature = lowerer.lower_type(node.type, node.span)->as<FunctionType>();

        register_function_symbol(signature->name, node.visibility, Linkage::Type::Internal,
                                 signature, node.span);

        return lowerer.lower_monomorphized_function(node, signature->name);
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        std::vector<HIRParameter> hir_parameters;
        hir_parameters.reserve(node.prototype->parameters.size());

        for (const auto& ast_parameter : node.prototype->parameters) {
            hir_parameters.emplace_back(ast_parameter->name,
                                        lowerer.lower_type(ast_parameter->type->type, node.span));
        }

        const auto variadic =
            !node.prototype->parameters.empty() && node.prototype->parameters.back()->is_variadic;
        const auto* signature = lowerer.lower_type(node.type, node.span);
        const auto* return_type = lowerer.lower_type(node.prototype->return_type->type, node.span);

        register_function_symbol(node.prototype->name, node.visibility, Linkage::Type::External,
                                 signature, node.span);

        return program.context.nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, node.prototype->name,
            std::move(hir_parameters), return_type, nullptr, variadic, signature);
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
        : sema(sema), resolver(sema.types, sema.env), mono_cache(),
          lowerer(program, resolver, mono_cache, sema, *this) {}

    HIRProgram lower(Program& program_node) {
        for (auto& statement : program_node.statements) {
            if (const auto* generic = statement->as<FunctionDeclaration>();
                generic != nullptr && !generic->prototype->generic_parameters.empty()) {
                mono_cache.register_function(generic->prototype->name, generic);
                continue;
            }

            if (auto* lowered = visit_statement(*statement); lowered != nullptr) {
                program.statements.push_back(static_cast<HIRStatement*>(lowered));
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

        for (const auto* statement : program.statements) {
            if (const auto* function = statement->as<HIRFunctionDeclaration>();
                function != nullptr) {
                register_function_symbol(function->name, function->visibility, function->linkage,
                                         function->type, function->span);
            }
        }

        return std::move(program);
    }
};
