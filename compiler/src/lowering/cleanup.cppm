module;

#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module zep.compiler.cleanup;

import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;
import zep.compiler.lowering.mangler;
import zep.hir.node;
import zep.hir.program;

class HIRDropAction {
  public:
    std::string source_name;
    std::string value_name;
    std::string flag_name;
    Span span;
    const Type* lowered_type;
    HIRDropAction(std::string source_name, std::string value_name, std::string flag_name, Span span,
                  const Type* lowered_type)
        : source_name(std::move(source_name)), value_name(std::move(value_name)),
          flag_name(std::move(flag_name)), span(span), lowered_type(lowered_type) {}
};

class HIRDeferAction {
  public:
    HIRStatement* statement;

    explicit HIRDeferAction(HIRStatement* statement) : statement(statement) {}
};

using HIRCleanupAction = std::variant<HIRDropAction, HIRDeferAction>;
using HIRCleanupScope = std::vector<HIRCleanupAction>;

export class HIRCleanupFrame {
  public:
    int return_temporary_counter = 0;
    int drop_flag_counter = 0;
    std::vector<HIRCleanupScope> scopes;
};

export class HIRCleanup {
  private:
    HIRProgram& program;
    SemaContext& sema;
    TypeResolver& resolver;

    HIRCleanupFrame frame;

    int closure_counter = 0;

    HIRVarDeclaration* flag_declaration(Span span, std::string flag_name) {
        const auto* boolean_type = sema.builtin_resolver.primitives.at("boolean");
        auto* initializer =
            program.context.nodes.create<HIRBooleanLiteral>(span, true, boolean_type);

        return program.context.nodes.create<HIRVarDeclaration>(
            span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            std::move(flag_name), boolean_type, initializer, false);
    }

  public:
    HIRCleanup(HIRProgram& program, SemaContext& sema, TypeResolver& resolver)
        : program(program), sema(sema), resolver(resolver) {}

    const FunctionSymbol* destructor_for(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* struct_type =
            resolved != nullptr ? resolved->as<StructType>() : type->as<StructType>();
        if (struct_type == nullptr) {
            return nullptr;
        }

        const auto* lifecycle = sema.lifecycles.find(struct_type);
        return lifecycle != nullptr ? lifecycle->destructor : nullptr;
    }

    bool needs_destruction(const Type* type) {
        if (type == nullptr) {
            return false;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* actual = resolved != nullptr ? resolved : type;
        if (const auto* array_type = actual->as<ArrayType>(); array_type != nullptr) {
            return needs_destruction(array_type->element);
        }

        const auto* struct_type = actual->as<StructType>();
        if (struct_type == nullptr) {
            const auto* enum_type = actual->as<EnumType>();
            if (enum_type == nullptr) {
                return false;
            }

            for (const auto& variant : enum_type->variants) {
                for (const auto& field : variant.fields) {
                    if (needs_destruction(field.type)) {
                        return true;
                    }
                }
            }
            return false;
        }

        if (destructor_for(struct_type) != nullptr) {
            return true;
        }

        for (const auto& field : struct_type->fields) {
            if (needs_destruction(field.type)) {
                return true;
            }
        }

        return false;
    }

    HIRExpression* copy_construct(Span span, HIRExpression* value, const Type* type) {
        if (value == nullptr || type == nullptr) {
            return value;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* struct_type = resolved != nullptr ? resolved->as<StructType>() : nullptr;
        const auto* lifecycle = sema.lifecycles.find(struct_type);
        const auto* constructor = lifecycle != nullptr ? lifecycle->copy_constructor : nullptr;
        if (constructor == nullptr || constructor->function_type == nullptr) {
            return value;
        }

        const auto* pointer_type = sema.types.create<PointerType>(resolved, false);
        std::vector<ParameterType> parameter_types;
        parameter_types.emplace_back("source", pointer_type);

        const auto* function_type = sema.types.create<FunctionType>(
            constructor->function_type->name, resolved, std::move(parameter_types),
            std::vector<GenericParameterType>(), false);
        auto name = Mangler::function_name(constructor->base_name(), constructor, function_type);

        auto* address = program.context.nodes.create<HIRUnaryExpression>(
            span, static_cast<UnaryOperator::Type>(UnaryExpression::Operator::Type::AddressOf),
            value, pointer_type);
        std::vector<HIRExpression*> arguments;
        arguments.reserve(1);
        arguments.push_back(address);

        return program.context.nodes.create<HIRCallExpression>(
            span,
            std::make_unique<HIRDirectCallTarget>(*constructor, std::move(name), function_type),
            std::move(arguments), resolved);
    }

    void build_drop_statements_for_expression(Span span, HIRExpression* target_expr,
                                              const Type* type,
                                              std::vector<HIRStatement*>& statements) {
        if (target_expr == nullptr || type == nullptr) {
            return;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* actual = resolved != nullptr ? resolved : type;
        if (const auto* array_type = actual->as<ArrayType>(); array_type != nullptr) {
            const auto* extent = std::get_if<ConcreteArrayExtent>(&array_type->extent);
            if (extent == nullptr) {
                return;
            }

            const auto* index_type = sema.builtin_resolver.primitives.at("i64");
            for (auto index = extent->value; index > 0; --index) {
                auto* index_value = program.context.nodes.create<HIRNumberLiteral>(
                    span, std::to_string(index - 1), index_type);
                auto* element = program.context.nodes.create<HIRIndexExpression>(
                    span, target_expr, index_value, array_type->element);
                build_drop_statements_for_expression(span, element, array_type->element,
                                                     statements);
            }
            return;
        }

        const auto* struct_type = actual->as<StructType>();
        if (struct_type == nullptr) {
            const auto* enum_type = actual->as<EnumType>();
            if (enum_type == nullptr) {
                return;
            }

            std::vector<HIRWhenArm> arms;
            arms.reserve(enum_type->variants.size());
            for (const auto& variant : enum_type->variants) {
                std::vector<HIRWhenPatternField> bindings;
                std::vector<HIRStatement*> body;
                bindings.reserve(variant.fields.size());

                for (auto index = variant.fields.size(); index > 0; --index) {
                    const auto& field = variant.fields[index - 1];
                    if (!needs_destruction(field.type)) {
                        continue;
                    }

                    auto binding_name = ".drop_payload_" +
                                        std::to_string(frame.drop_flag_counter++);
                    bindings.emplace_back(field.name, binding_name, field.type);
                    auto* value = program.context.nodes.create<HIRIdentifierExpression>(
                        span, std::move(binding_name), field.type);
                    build_drop_statements_for_expression(span, value, field.type, body);
                }

                auto* block =
                    program.context.nodes.create<HIRBlockStatement>(span, std::move(body));
                std::vector<HIRWhenPattern> patterns;
                patterns.reserve(1);
                patterns.emplace_back(enum_type->name, variant.name, variant.index, nullptr,
                                      std::move(bindings));
                arms.emplace_back(false, std::move(patterns), nullptr, block);
            }

            const auto* void_type = sema.builtin_resolver.primitives.at("void");
            auto* cleanup = program.context.nodes.create<HIRWhenExpression>(
                span, target_expr, std::move(arms), true, void_type);
            statements.push_back(
                program.context.nodes.create<HIRExpressionStatement>(span, cleanup));
            return;
        }

        const auto* destructor = destructor_for(struct_type);
        if (destructor != nullptr) {
            const auto* generic_function_type = destructor->function_type;
            if (generic_function_type != nullptr) {
                const auto* pointer_type = sema.types.create<PointerType>(struct_type, true);

                std::vector<ParameterType> parameter_types;
                parameter_types.emplace_back("self", pointer_type);

                const auto* function_type = sema.types.create<FunctionType>(
                    generic_function_type->name, generic_function_type->return_type,
                    std::move(parameter_types), std::vector<GenericParameterType>(), false);

                auto base_name = destructor->base_name();
                auto name = Mangler::function_name(base_name, destructor, function_type);

                auto* address = program.context.nodes.create<HIRUnaryExpression>(
                    span,
                    static_cast<UnaryOperator::Type>(UnaryExpression::Operator::Type::AddressOfMut),
                    target_expr, pointer_type);

                std::vector<HIRExpression*> arguments;
                arguments.reserve(1);
                arguments.push_back(address);

                auto* call = program.context.nodes.create<HIRCallExpression>(
                    span, std::make_unique<HIRDirectCallTarget>(*destructor, std::move(name),
                                                               function_type),
                    std::move(arguments), function_type->return_type);

                statements.push_back(
                    program.context.nodes.create<HIRExpressionStatement>(span, call));
            }
        }

        const auto inherited_count =
            struct_type->base_type != nullptr ? struct_type->base_type->fields.size() : 0;
        for (auto index = struct_type->fields.size(); index > inherited_count; --index) {
            const auto& field = struct_type->fields[index - 1];
            if (needs_destruction(field.type)) {
                auto* field_expr = program.context.nodes.create<HIRMemberExpression>(
                    span, target_expr, field.name, field.type);
                build_drop_statements_for_expression(span, field_expr, field.type, statements);
            }
        }

        if (struct_type->base_type != nullptr && needs_destruction(struct_type->base_type)) {
            build_drop_statements_for_expression(span, target_expr, struct_type->base_type,
                                                 statements);
        }
    }

    HIRStatement* build_drop(const HIRDropAction& action) {
        std::vector<HIRStatement*> drop_statements;
        auto* base_expr = program.context.nodes.create<HIRIdentifierExpression>(
            action.span, action.value_name, action.lowered_type);

        build_drop_statements_for_expression(action.span, base_expr, action.lowered_type,
                                             drop_statements);

        if (drop_statements.empty()) {
            return nullptr;
        }

        HIRStatement* destructor_call = nullptr;
        if (drop_statements.size() == 1) {
            destructor_call = drop_statements.front();
        } else {
            destructor_call = program.context.nodes.create<HIRBlockStatement>(
                action.span, std::move(drop_statements));
        }

        const auto* boolean_type = sema.builtin_resolver.primitives.at("boolean");
        const auto* void_type = sema.builtin_resolver.primitives.at("void");

        auto* condition = program.context.nodes.create<HIRIdentifierExpression>(
            action.span, action.flag_name, boolean_type);
        auto* cleanup = program.context.nodes.create<HIRIfExpression>(
            action.span, condition, destructor_call, nullptr, void_type);

        return program.context.nodes.create<HIRExpressionStatement>(action.span, cleanup);
    }

    HIRStatement* build_action(const HIRCleanupAction& action) {
        if (const auto* drop = std::get_if<HIRDropAction>(&action); drop != nullptr) {
            return build_drop(*drop);
        }

        const auto* deferred = std::get_if<HIRDeferAction>(&action);
        return deferred != nullptr ? deferred->statement : nullptr;
    }

    void emit_scope_into(const HIRCleanupScope& scope, std::vector<HIRStatement*>& out) {
        for (const auto& action : scope | std::views::reverse) {
            if (auto* statement = build_action(action); statement != nullptr) {
                out.push_back(statement);
            }
        }
    }

    void push_scope() { frame.scopes.emplace_back(); }

    void pop_scope() { frame.scopes.pop_back(); }

    HIRVarDeclaration* register_var(VarDeclaration& node, std::string value_name,
                                    const Type* lowered_type) {
        if (frame.scopes.empty() || node.type == nullptr || lowered_type == nullptr) {
            return nullptr;
        }

        if (!needs_destruction(node.type)) {
            return nullptr;
        }

        auto flag_name = ".drop_" + node.name + "_" + std::to_string(frame.drop_flag_counter++);
        auto declaration_name = flag_name;

        frame.scopes.back().emplace_back(std::in_place_type<HIRDropAction>, node.name,
                                         std::move(value_name), std::move(flag_name), node.span,
                                         lowered_type);

        return flag_declaration(node.span, std::move(declaration_name));
    }

    HIRVarDeclaration* register_parameter(const std::string& name, Span span, const Type* type,
                                          const Type* lowered_type) {
        if (frame.scopes.empty() || type == nullptr || lowered_type == nullptr) {
            return nullptr;
        }

        if (!needs_destruction(type)) {
            return nullptr;
        }

        auto flag_name = ".drop_" + name + "_" + std::to_string(frame.drop_flag_counter++);
        auto declaration_name = flag_name;

        frame.scopes.back().emplace_back(std::in_place_type<HIRDropAction>, name, name,
                                         std::move(flag_name), span, lowered_type);

        return flag_declaration(span, std::move(declaration_name));
    }

    void register_defer(HIRStatement* statement) {
        if (frame.scopes.empty() || statement == nullptr) {
            return;
        }

        frame.scopes.back().emplace_back(std::in_place_type<HIRDeferAction>, statement);
    }

    std::string drop_flag_for(const std::string& name) {
        for (auto& scope : frame.scopes | std::views::reverse) {
            for (auto& action : scope | std::views::reverse) {
                auto* drop = std::get_if<HIRDropAction>(&action);
                if (drop != nullptr && drop->source_name == name) {
                    return drop->flag_name;
                }
            }
        }

        return {};
    }

    HIRStatement* emit_explicit_drop(Span span, const std::string& name, HIRExpression* target,
                                     const Type* type) {
        std::vector<HIRStatement*> statements;
        build_drop_statements_for_expression(span, target, type, statements);

        auto flag_name = drop_flag_for(name);
        if (!flag_name.empty()) {
            const auto* boolean_type = sema.builtin_resolver.primitives.at("boolean");
            auto* flag = program.context.nodes.create<HIRIdentifierExpression>(
                span, std::move(flag_name), boolean_type);
            auto* cleared =
                program.context.nodes.create<HIRBooleanLiteral>(span, false, boolean_type);
            auto* assignment = program.context.nodes.create<HIRAssignExpression>(
                span, flag, cleared, boolean_type);
            statements.push_back(
                program.context.nodes.create<HIRExpressionStatement>(span, assignment));
        }

        if (statements.empty()) {
            return nullptr;
        }
        if (statements.size() == 1) {
            return statements.front();
        }
        return program.context.nodes.create<HIRStatementGroup>(span, std::move(statements));
    }

    std::vector<HIRStatement*> emit_scope() {
        std::vector<HIRStatement*> statements;
        if (frame.scopes.empty()) {
            return statements;
        }

        statements.reserve(frame.scopes.back().size());
        emit_scope_into(frame.scopes.back(), statements);

        return statements;
    }

    std::vector<HIRStatement*> emit_all() {
        std::vector<HIRStatement*> statements;

        auto total = std::size_t{0};
        for (const auto& scope : frame.scopes) {
            total += scope.size();
        }
        statements.reserve(total);

        for (const auto& scope : frame.scopes | std::views::reverse) {
            emit_scope_into(scope, statements);
        }

        return statements;
    }

    HIRCleanupFrame enter_function() {
        HIRCleanupFrame saved;
        saved.return_temporary_counter = std::exchange(frame.return_temporary_counter, 0);
        saved.drop_flag_counter = std::exchange(frame.drop_flag_counter, 0);
        saved.scopes = std::move(frame.scopes);
        frame.scopes.clear();
        return saved;
    }

    void leave_function(HIRCleanupFrame saved) {
        frame.return_temporary_counter = saved.return_temporary_counter;
        frame.drop_flag_counter = saved.drop_flag_counter;
        frame.scopes = std::move(saved.scopes);
    }

    std::string next_closure_name() { return ".closure_" + std::to_string(closure_counter++); }

    std::string next_return_tmp_name() {
        return ".return_tmp_" + std::to_string(frame.return_temporary_counter++);
    }
};

export class CleanupFunctionGuard {
  private:
    HIRCleanup& cleanup;
    HIRCleanupFrame saved;

  public:
    explicit CleanupFunctionGuard(HIRCleanup& cleanup)
        : cleanup(cleanup), saved(cleanup.enter_function()) {}

    CleanupFunctionGuard(const CleanupFunctionGuard&) = delete;
    CleanupFunctionGuard& operator=(const CleanupFunctionGuard&) = delete;

    ~CleanupFunctionGuard() { cleanup.leave_function(std::move(saved)); }
};

export class CleanupScopeGuard {
  private:
    HIRCleanup& cleanup;

  public:
    explicit CleanupScopeGuard(HIRCleanup& cleanup) : cleanup(cleanup) { cleanup.push_scope(); }

    CleanupScopeGuard(const CleanupScopeGuard&) = delete;
    CleanupScopeGuard& operator=(const CleanupScopeGuard&) = delete;

    ~CleanupScopeGuard() { cleanup.pop_scope(); }
};
