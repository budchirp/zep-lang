module;

#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
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
    const FunctionSymbol* destructor;

    HIRDropAction(std::string source_name, std::string value_name, std::string flag_name, Span span,
                  const Type* lowered_type, const FunctionSymbol* destructor)
        : source_name(std::move(source_name)), value_name(std::move(value_name)),
          flag_name(std::move(flag_name)), span(span), lowered_type(lowered_type),
          destructor(destructor) {}
};

class HIRDeferAction {
  public:
    Span span;
    HIRStatement* statement;

    HIRDeferAction(Span span, HIRStatement* statement) : span(span), statement(statement) {}
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

    std::unordered_map<std::string, const FunctionSymbol*> destructors;
    HIRCleanupFrame frame;

    int closure_counter = 0;

    void collect_from(StructDeclaration& node) {
        for (auto* method : node.methods) {
            if (method->kind() == FunctionSymbol::Kind::Type::Destructor &&
                method->function_symbol != nullptr) {
                destructors[node.name] = method->function_symbol;
            }
        }

        for (auto* nested : node.structs) {
            collect_from(*nested);
        }
    }

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

        if (auto iterator = destructors.find(struct_type->name); iterator != destructors.end()) {
            return iterator->second;
        }

        auto base_name = struct_type->name;
        if (auto angle = base_name.find('<'); angle != std::string::npos) {
            base_name = base_name.substr(0, angle);
        }
        if (auto dot = base_name.rfind('.'); dot != std::string::npos) {
            base_name = base_name.substr(dot + 1);
        }

        if (auto iterator = destructors.find(base_name); iterator != destructors.end()) {
            return iterator->second;
        }

        if (const auto* symbol = sema.env.current_scope->lookup_type(struct_type->name);
            symbol != nullptr && symbol->member_scope != nullptr) {
            if (const auto* function_symbol =
                    symbol->member_scope->find_local_function("~" + struct_type->name);
                function_symbol != nullptr) {
                return function_symbol;
            }
        }

        if (const auto* symbol = sema.env.current_scope->lookup_type(base_name);
            symbol != nullptr && symbol->member_scope != nullptr) {
            if (const auto* function_symbol =
                    symbol->member_scope->find_local_function("~" + base_name);
                function_symbol != nullptr) {
                return function_symbol;
            }
        }

        return nullptr;
    }

    bool needs_destruction(const Type* type) {
        if (type == nullptr) {
            return false;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* struct_type =
            resolved != nullptr ? resolved->as<StructType>() : type->as<StructType>();
        if (struct_type == nullptr) {
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

    void build_drop_statements_for_expression(Span span, HIRExpression* target_expr,
                                              const Type* type,
                                              std::vector<HIRStatement*>& statements) {
        if (target_expr == nullptr || type == nullptr) {
            return;
        }

        const auto* resolved = resolver.resolve_type(type);
        const auto* struct_type =
            resolved != nullptr ? resolved->as<StructType>() : type->as<StructType>();
        if (struct_type == nullptr) {
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
                    span, std::make_unique<HIRDirectCallTarget>(*destructor, std::string()),
                    std::move(arguments), function_type->return_type);

                statements.push_back(
                    program.context.nodes.create<HIRExpressionStatement>(span, call));
            }
        }

        for (const auto& field : struct_type->fields | std::views::reverse) {
            if (needs_destruction(field.type)) {
                auto* field_expr = program.context.nodes.create<HIRMemberExpression>(
                    span, target_expr, field.name, field.type);
                build_drop_statements_for_expression(span, field_expr, field.type, statements);
            }
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

    void collect_destructors(Program& program_node) {
        destructors.clear();

        for (auto* statement : program_node.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                collect_from(*struct_declaration);
            }
        }
    }

    void collect_destructors(const std::vector<const Program*>& programs) {
        destructors.clear();

        for (const auto* program_node : programs) {
            if (program_node == nullptr) {
                continue;
            }

            for (auto* statement : program_node->statements) {
                if (auto* struct_declaration = statement->as<StructDeclaration>();
                    struct_declaration != nullptr) {
                    collect_from(*struct_declaration);
                }
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

        const auto* destructor = destructor_for(node.type);
        auto flag_name = ".drop_" + node.name + "_" + std::to_string(frame.drop_flag_counter++);
        auto declaration_name = flag_name;

        frame.scopes.back().emplace_back(std::in_place_type<HIRDropAction>, node.name,
                                         std::move(value_name), std::move(flag_name), node.span,
                                         lowered_type, destructor);

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

        const auto* destructor = destructor_for(type);
        auto flag_name = ".drop_" + name + "_" + std::to_string(frame.drop_flag_counter++);
        auto declaration_name = flag_name;

        frame.scopes.back().emplace_back(std::in_place_type<HIRDropAction>, name, name,
                                         std::move(flag_name), span, lowered_type, destructor);

        return flag_declaration(span, std::move(declaration_name));
    }

    void register_defer(Span span, HIRStatement* statement) {
        if (frame.scopes.empty() || statement == nullptr) {
            return;
        }

        frame.scopes.back().emplace_back(std::in_place_type<HIRDeferAction>, span, statement);
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

    HIRExpression* clear_after(Span span, const std::string& name, HIRExpression* value,
                               const Type* type) {
        auto flag_name = drop_flag_for(name);
        if (flag_name.empty()) {
            return value;
        }

        return program.context.nodes.create<HIRDropFlagClearExpression>(span, value,
                                                                        std::move(flag_name), type);
    }

    HIRExpression* clear_after_destructor_call(CallExpression& node, HIRExpression* expression) {
        if (node.arguments.empty()) {
            return expression;
        }

        auto* value = node.arguments.front()->value;
        if (auto* unary = value->as<UnaryExpression>(); unary != nullptr) {
            value = unary->operand;
        }

        auto* identifier = value->as<IdentifierExpression>();
        if (identifier == nullptr) {
            return expression;
        }

        return clear_after(node.span, identifier->name, expression,
                           expression != nullptr ? expression->type : nullptr);
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
