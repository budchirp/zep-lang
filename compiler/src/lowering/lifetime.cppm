module;

#include <functional>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <variant>

export module zep.compiler.lowering.lifetime;

import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.context;
import zep.frontend.sema.lifecycle;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.compiler.lowering.mangler;
import zep.compiler.lowering.specialization;
import zep.hir.node;
import zep.hir.program;

export class TemporaryNames {
  private:
    std::size_t counter;

  public:
    TemporaryNames() : counter(0) {}

    std::string next(std::string_view prefix) {
        return "." + std::string(prefix) + "_" + std::to_string(counter++);
    }
};

export class Construction {
  private:
    HIRProgram& program;
    SemaContext& sema;
    TypeResolver& resolver;
    TemporaryNames& names;
    Specializations& specializations;
    std::function<const Type*(const Type*, Span, Operation::Type)> prepare_type;

    HIRExpression* base(Span span, HIRExpression* value, const Type* type) {
        const auto* source = sema.types.create<PointerType>(value->type, true);
        const auto* target = sema.types.create<PointerType>(type, true);
        auto* address = program.nodes.create<HIRUnaryExpression>(span,
            UnaryOperator::Type::AddressOfMut, value, source);
        auto* pointer = program.nodes.create<HIRCoerceExpression>(span, address, target,
            Coercion::Type::None, source);

        return program.nodes.create<HIRUnaryExpression>(span,
            UnaryOperator::Type::Dereference, pointer, type);
    }

  public:
    Construction(HIRProgram& program, SemaContext& sema, TypeResolver& resolver, TemporaryNames& names,
                 Specializations& specializations,
                 std::function<const Type*(const Type*, Span, Operation::Type)> prepare_type)
        : program(program), sema(sema), resolver(resolver), names(names), specializations(specializations),
          prepare_type(std::move(prepare_type)) {}

    HIRExpression* construct(Span span, HIRExpression* value,
                             std::shared_ptr<const LifecyclePlan> plan) {
        if (value == nullptr || plan == nullptr || plan->type == nullptr) {
            return value;
        }

        const auto* type = prepare_type(plan->type, span, plan->operation);

        if (plan->action == LifecyclePlan::Action::Type::Deferred) {
            plan = sema.lifecycle.resolve(type, plan->operation, resolver);
        }

        if (plan->action == LifecyclePlan::Action::Type::Deleted) {
            throw std::runtime_error("use of deleted lifecycle operation for '" + type->to_string() + "'");
        }

        if (plan->action == LifecyclePlan::Action::Type::Trivial) {
            return value;
        }

        if (plan->action == LifecyclePlan::Action::Type::Function) {
            const auto* function = plan->function;
            const auto* source = function->signature()->parameters.front().type->as<PointerType>();
            const auto* pointer = sema.types.create<PointerType>(type, source->is_mutable, source->reference_kind);
            std::vector<ParameterType> parameters;
            parameters.reserve(1);
            parameters.emplace_back("source", pointer);
            const auto* signature = sema.types.create<FunctionType>(function->signature()->name,
                type, std::move(parameters), std::vector<GenericParameterType>(), false);
            auto name = specializations.lifecycle(*function, *type->as_nominal(), signature).name;
            std::vector<HIRExpression*> arguments;
            arguments.reserve(1);
            arguments.push_back(program.nodes.create<HIRUnaryExpression>(span,
                source->is_mutable ? UnaryOperator::Type::AddressOfMut : UnaryOperator::Type::AddressOf,
                value, pointer));

            return program.nodes.create<HIRCallExpression>(span,
                std::make_unique<HIRDirectCallTarget>(*function, std::move(name), signature),
                std::move(arguments), type);
        }

        auto name = names.next("return_tmp");
        const auto* pointer = sema.types.create<PointerType>(type, true);
        auto* address = program.nodes.create<HIRUnaryExpression>(span,
            UnaryOperator::Type::AddressOfMut, value, pointer);
        auto* source = program.nodes.create<HIRUnaryExpression>(span,
            UnaryOperator::Type::Dereference,
            program.nodes.create<HIRIdentifierExpression>(span, name, pointer), type);
        std::vector<HIRStatement*> statements;
        statements.reserve(plan->parts.size() + 2);
        statements.push_back(program.nodes.create<HIRVarDeclaration>(span,
            Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            std::move(name), pointer, address, false));
        HIRExpression* result = nullptr;

        if (const auto* structure = type->as<StructType>(); structure != nullptr) {
            std::vector<HIRStructLiteralField> fields;
            fields.reserve(structure->fields.size());

            for (const auto& part : plan->parts) {
                const auto* field_type = prepare_type(part.plan->type, span, part.plan->operation);

                if (part.kind == LifecyclePart::Kind::Type::Base) {
                    auto base_name = names.next("return_tmp");
                    auto* copied = construct(span, base(span, source, field_type), part.plan);
                    statements.push_back(program.nodes.create<HIRVarDeclaration>(span,
                        Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                        base_name, field_type, copied, false));
                    auto* stored = program.nodes.create<HIRIdentifierExpression>(span, base_name, field_type);

                    for (const auto& field : field_type->as<StructType>()->fields) {
                        fields.emplace_back(field.name, program.nodes.create<HIRMemberExpression>(
                            span, stored, field.name, field.type));
                    }
                } else {
                    auto* field = program.nodes.create<HIRMemberExpression>(span, source, part.name, field_type);
                    fields.emplace_back(part.name, construct(span, field, part.plan));
                }
            }

            result = program.nodes.create<HIRStructLiteralExpression>(span, structure->name,
                std::move(fields), type);
        } else if (const auto* array = type->as<ArrayType>(); array != nullptr) {
            const auto& part = plan->parts.front();
            std::vector<HIRExpression*> elements;
            elements.reserve(part.count);

            for (auto index = std::size_t{0}; index < part.count; ++index) {
                auto* offset = program.nodes.create<HIRNumberLiteral>(span, std::to_string(index),
                    sema.primitives.entries.at("i64"));
                auto* element = program.nodes.create<HIRIndexExpression>(span, source, offset, array->element);
                elements.push_back(construct(span, element, part.plan));
            }

            result = program.nodes.create<HIRArrayLiteralExpression>(span, std::move(elements), type);
        } else if (const auto* enumeration = type->as<EnumType>(); enumeration != nullptr) {
            std::vector<HIRWhenArm> arms;
            arms.reserve(enumeration->variants.size());

            for (const auto& variant : enumeration->variants) {
                std::vector<HIRWhenPatternField> bindings;
                std::vector<HIRStructLiteralField> fields;
                bindings.reserve(variant.fields.size());
                fields.reserve(variant.fields.size());

                for (const auto& part : plan->parts) {
                    if (part.variant != variant.index) {
                        continue;
                    }

                    auto binding = names.next("return_tmp");
                    const auto* field_type = prepare_type(part.plan->type, span, part.plan->operation);
                    bindings.emplace_back(part.name, binding, field_type);
                    auto* field = program.nodes.create<HIRIdentifierExpression>(span, binding, field_type);
                    fields.emplace_back(part.name, construct(span, field, part.plan));
                }

                auto* payload = program.nodes.create<HIREnumVariantExpression>(span,
                    enumeration->name, variant.name, std::move(fields), variant.index, type);
                std::vector<HIRWhenPattern> patterns;
                patterns.reserve(1);
                patterns.emplace_back(enumeration->name, variant.name, variant.index, nullptr,
                                      std::move(bindings));
                arms.emplace_back(false, std::move(patterns), nullptr,
                    program.nodes.create<HIRExpressionStatement>(span, payload));
            }

            auto* selection = program.nodes.create<HIRWhenExpression>(span, source,
                std::move(arms), true, type);
            selection->borrow_payloads = true;
            result = selection;
        }

        statements.push_back(program.nodes.create<HIRExpressionStatement>(span, result));

        return program.nodes.create<HIRBlockExpression>(span,
            program.nodes.create<HIRBlockStatement>(span, std::move(statements)), type);
    }

    void destroy(Span span, HIRExpression* value, std::shared_ptr<const LifecyclePlan> plan,
                 std::vector<HIRStatement*>& statements) {
        if (value == nullptr || plan == nullptr || plan->type == nullptr) {
            return;
        }

        const auto* type = prepare_type(plan->type, span, Operation::Type::Destroy);

        if (plan->action == LifecyclePlan::Action::Type::Deferred) {
            plan = sema.lifecycle.resolve(type, Operation::Type::Destroy, resolver);
        }

        if (plan->action == LifecyclePlan::Action::Type::Trivial) {
            return;
        }

        if (plan->action == LifecyclePlan::Action::Type::Deleted) {
            throw std::runtime_error("use of deleted destructor for '" + type->to_string() + "'");
        }

        if (const auto* function = plan->function; function != nullptr) {
            const auto* pointer = sema.types.create<PointerType>(type, true);
            std::vector<ParameterType> parameters;
            parameters.reserve(1);
            parameters.emplace_back("self", pointer);
            const auto* signature = sema.types.create<FunctionType>(function->signature()->name,
                function->signature()->return_type, std::move(parameters),
                std::vector<GenericParameterType>(), false);
            std::vector<HIRExpression*> arguments;
            arguments.reserve(1);
            arguments.push_back(program.nodes.create<HIRUnaryExpression>(span,
                UnaryOperator::Type::AddressOfMut, value, pointer));
            auto* call = program.nodes.create<HIRCallExpression>(span,
                std::make_unique<HIRDirectCallTarget>(*function,
                    specializations.lifecycle(*function, *type->as_nominal(), signature).name, signature),
                std::move(arguments), signature->return_type);
            statements.push_back(program.nodes.create<HIRExpressionStatement>(span, call));
        }

        if (const auto* enumeration = type->as<EnumType>(); enumeration != nullptr) {
            std::vector<HIRWhenArm> arms;
            arms.reserve(enumeration->variants.size());

            for (const auto& variant : enumeration->variants) {
                std::vector<HIRWhenPatternField> bindings;
                std::vector<HIRStatement*> body;
                bindings.reserve(variant.fields.size());
                body.reserve(variant.fields.size());

                for (const auto& part : plan->parts | std::views::reverse) {
                    if (part.variant != variant.index || part.plan->action == LifecyclePlan::Action::Type::Trivial) {
                        continue;
                    }

                    auto name = names.next("drop_payload");
                    const auto* field_type = prepare_type(part.plan->type, span, Operation::Type::Destroy);
                    bindings.emplace_back(part.name, name, field_type);
                    auto* field = program.nodes.create<HIRIdentifierExpression>(span, name, field_type);
                    destroy(span, field, part.plan, body);
                }

                std::vector<HIRWhenPattern> patterns;
                patterns.reserve(1);
                patterns.emplace_back(enumeration->name, variant.name, variant.index, nullptr,
                                      std::move(bindings));
                arms.emplace_back(false, std::move(patterns), nullptr,
                    program.nodes.create<HIRBlockStatement>(span, std::move(body)));
            }

            auto* selection = program.nodes.create<HIRWhenExpression>(span, value,
                std::move(arms), true, sema.primitives.entries.at("void"));
            selection->borrow_payloads = true;
            statements.push_back(program.nodes.create<HIRExpressionStatement>(span, selection));

            return;
        }

        for (const auto& part : plan->parts | std::views::reverse) {
            const auto* field_type = prepare_type(part.plan->type, span, Operation::Type::Destroy);

            if (part.kind == LifecyclePart::Kind::Type::Base) {
                destroy(span, base(span, value, field_type), part.plan, statements);
            } else if (part.kind == LifecyclePart::Kind::Type::Element) {
                for (auto index = part.count; index > 0; --index) {
                    auto* offset = program.nodes.create<HIRNumberLiteral>(span, std::to_string(index - 1),
                        sema.primitives.entries.at("i64"));
                    auto* element = program.nodes.create<HIRIndexExpression>(span, value, offset, field_type);
                    destroy(span, element, part.plan, statements);
                }
            } else {
                auto* field = program.nodes.create<HIRMemberExpression>(span, value, part.name, field_type);
                destroy(span, field, part.plan, statements);
            }
        }
    }
};

export class DropAction {
  public:
    std::string source;
    std::string name;
    std::string flag;
    Span span;
    std::shared_ptr<const LifecyclePlan> plan;

    DropAction(std::string source, std::string name, std::string flag, Span span,
               std::shared_ptr<const LifecyclePlan> plan)
        : source(std::move(source)), name(std::move(name)), flag(std::move(flag)), span(span),
          plan(std::move(plan)) {}
};

export using CleanupAction = std::variant<DropAction, HIRStatement*>;
export using CleanupScope = std::vector<CleanupAction>;

export class Cleanup {
  private:
    HIRProgram& program;
    SemaContext& sema;
    TypeResolver& resolver;
    Construction& construction;
    TemporaryNames& names;

  public:
    class Extent {
      public:
        enum class Type : std::uint8_t { Scope, Function };
    };

    class Initialization {
      public:
        enum class Type : std::uint8_t { Initialized, Uninitialized };
    };

    std::vector<CleanupScope> scopes;

    Cleanup(HIRProgram& program, SemaContext& sema, TypeResolver& resolver,
            Construction& construction, TemporaryNames& names)
        : program(program), sema(sema), resolver(resolver), construction(construction),
          names(names), scopes() {}

    HIRVarDeclaration* register_value(std::string source, std::string name, Span span, const Type* type,
                                      CleanupScope* actions = nullptr,
                                      Initialization::Type state = Initialization::Type::Initialized) {
        if (type == nullptr) {
            return nullptr;
        }

        if (actions == nullptr) {
            if (scopes.empty()) {
                return nullptr;
            }

            actions = &scopes.back();
        }

        auto plan = sema.lifecycle.resolve(type, Operation::Type::Destroy, resolver);

        if (plan->action == LifecyclePlan::Action::Type::Trivial) {
            return nullptr;
        }

        auto flag = names.next("drop_" + source);
        actions->emplace_back(std::in_place_type<DropAction>, std::move(source),
                              std::move(name), flag, span, std::move(plan));
        const auto* boolean = sema.primitives.entries.at("boolean");

        return program.nodes.create<HIRVarDeclaration>(span, Visibility::Type::Private,
            Linkage::Type::Internal, StorageKind::Type::VarMut, std::move(flag), boolean,
            program.nodes.create<HIRBooleanLiteral>(span, state == Initialization::Type::Initialized, boolean), false);
    }

    void defer(HIRStatement* statement) {
        if (!scopes.empty() && statement != nullptr) {
            scopes.back().emplace_back(statement);
        }
    }

    std::string flag(const std::string& source) const {
        for (const auto& scope : scopes | std::views::reverse) {
            for (const auto& action : scope | std::views::reverse) {
                const auto* drop = std::get_if<DropAction>(&action);

                if (drop != nullptr && drop->source == source) {
                    return drop->flag;
                }
            }
        }

        return {};
    }

    HIRStatement* destroy(Span span, const std::string& source, HIRExpression* value,
                          const Type* type, std::shared_ptr<const LifecyclePlan> plan) {
        std::vector<HIRStatement*> statements;
        statements.reserve(3);

        const auto* pointer_type = sema.types.create<PointerType>(type, true);
        auto name = names.next("drop_target");
        auto* address = program.nodes.create<HIRUnaryExpression>(
            span, UnaryOperator::Type::AddressOfMut, value, pointer_type);
        statements.push_back(program.nodes.create<HIRVarDeclaration>(span,
            Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            name, pointer_type, address, false));
        auto* pointer = program.nodes.create<HIRIdentifierExpression>(span, name, pointer_type);
        auto* target = program.nodes.create<HIRUnaryExpression>(
            span, UnaryOperator::Type::Dereference, pointer, type);

        construction.destroy(span, target, std::move(plan), statements);

        auto flag_name = flag(source);

        if (!flag_name.empty()) {
            const auto* boolean = sema.primitives.entries.at("boolean");
            auto* target = program.nodes.create<HIRIdentifierExpression>(span, std::move(flag_name), boolean);
            auto* cleared = program.nodes.create<HIRBooleanLiteral>(span, false, boolean);
            auto* assignment = program.nodes.create<HIRAssignExpression>(span, target, cleared, boolean);
            statements.push_back(program.nodes.create<HIRExpressionStatement>(span, assignment));
        }

        return program.nodes.create<HIRStatementGroup>(span, std::move(statements));
    }

    void emit(const CleanupScope& scope, std::vector<HIRStatement*>& statements) {
        for (const auto& action : scope | std::views::reverse) {
            if (const auto* drop = std::get_if<DropAction>(&action); drop != nullptr) {
                std::vector<HIRStatement*> body;
                body.reserve(drop->plan->parts.size() + 1);
                auto* value = program.nodes.create<HIRIdentifierExpression>(drop->span,
                    drop->name, drop->plan->type);
                construction.destroy(drop->span, value, drop->plan, body);
                auto* condition = program.nodes.create<HIRIdentifierExpression>(drop->span,
                    drop->flag, sema.primitives.entries.at("boolean"));
                auto* destruction = program.nodes.create<HIRIfExpression>(drop->span, condition,
                    program.nodes.create<HIRBlockStatement>(drop->span, std::move(body)), nullptr,
                    sema.primitives.entries.at("void"));
                statements.push_back(program.nodes.create<HIRExpressionStatement>(drop->span, destruction));
            } else {
                statements.push_back(std::get<HIRStatement*>(action));
            }
        }
    }

    std::vector<HIRStatement*> emit(Extent::Type extent = Extent::Type::Scope) {
        std::vector<HIRStatement*> statements;
        auto count = std::size_t{0};

        for (const auto& scope : scopes) {
            count += scope.size();
        }

        statements.reserve(count);

        for (const auto& scope : scopes | std::views::reverse) {
            emit(scope, statements);

            if (extent == Extent::Type::Scope) {
                break;
            }
        }

        return statements;
    }
};

export class CleanupScopeGuard {
  private:
    Cleanup& cleanup;

  public:
    explicit CleanupScopeGuard(Cleanup& cleanup) : cleanup(cleanup) {
        cleanup.scopes.emplace_back();
    }

    CleanupScopeGuard(const CleanupScopeGuard&) = delete;
    CleanupScopeGuard& operator=(const CleanupScopeGuard&) = delete;

    ~CleanupScopeGuard() {
        cleanup.scopes.pop_back();
    }
};

export class FullExpression {
  public:
    FullExpression* parent;
    std::vector<HIRStatement*> declarations;
    std::vector<HIRStatement*> drops;

    explicit FullExpression(FullExpression* parent) : parent(parent), declarations(), drops() {
        declarations.reserve(4);
        drops.reserve(4);
    }
};

export class Temporaries {
  private:
    HIRProgram& program;
    SemaContext& sema;
    Cleanup& cleanup;
    TemporaryNames& names;

  public:
    FullExpression* current;

    Temporaries(HIRProgram& program, SemaContext& sema, Cleanup& cleanup, TemporaryNames& names)
        : program(program), sema(sema), cleanup(cleanup), names(names), current(nullptr) {}

    HIRExpression* finish(Span span, HIRExpression* value, std::vector<HIRStatement*>& statements,
                           const std::vector<HIRStatement*>& actions) {
        if (value != nullptr && !value->type->is<VoidType>() && !value->type->is<NeverType>()) {
            auto name = names.next("result");
            statements.push_back(program.nodes.create<HIRVarDeclaration>(span,
                Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                name, value->type, value, false));
            value = program.nodes.create<HIRIdentifierExpression>(span, name, value->type);
        } else if (value != nullptr) {
            statements.push_back(program.nodes.create<HIRExpressionStatement>(span, value));
            value = nullptr;
        }

        statements.insert(statements.end(), actions.begin(), actions.end());

        return value;
    }

    HIRExpression* complete(Span span, HIRExpression* value, FullExpression& expression) {
        if (expression.declarations.empty() || value == nullptr) {
            return value;
        }

        const auto* type = value->type;
        value = finish(span, value, expression.declarations, expression.drops);

        if (value != nullptr) {
            expression.declarations.push_back(program.nodes.create<HIRExpressionStatement>(span, value));
        }

        auto* body = program.nodes.create<HIRBlockStatement>(span, std::move(expression.declarations));

        return program.nodes.create<HIRBlockExpression>(span, body, type);
    }

    HIRExpression* capture(HIRExpression* value, Span span) {
        if (current == nullptr || value == nullptr || value->type == nullptr ||
            value->as<HIRIdentifierExpression>() != nullptr || value->as<HIRMemberExpression>() != nullptr ||
            value->as<HIRIndexExpression>() != nullptr) {
            return value;
        }

        if (const auto* unary = value->as<HIRUnaryExpression>(); unary != nullptr &&
            unary->op == UnaryOperator::Type::Dereference) {
            return value;
        }

        const auto* type = value->type;
        CleanupScope actions;
        actions.reserve(1);
        auto name = names.next("temporary");
        auto* declaration = cleanup.register_value({}, name, span, type, &actions,
                                                   Cleanup::Initialization::Type::Uninitialized);

        if (declaration == nullptr) {
            return value;
        }

        const auto* pointer_type = sema.types.create<PointerType>(type, true);
        const auto* boolean = sema.primitives.entries.at("boolean");
        auto* storage = program.nodes.create<HIRIdentifierExpression>(span, name, type);
        auto* flag = program.nodes.create<HIRIdentifierExpression>(span, declaration->name, boolean);
        current->declarations.push_back(program.nodes.create<HIRVarDeclaration>(span,
            Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::VarMut,
            name, type, nullptr, false));
        current->declarations.push_back(declaration);

        std::vector<HIRStatement*> drops;
        drops.reserve(1);
        cleanup.emit(actions, drops);
        current->drops.insert(current->drops.begin(), drops.begin(), drops.end());

        std::vector<HIRStatement*> statements;
        statements.reserve(3);
        auto* initialize = program.nodes.create<HIRAssignExpression>(span, storage, value, type);
        statements.push_back(program.nodes.create<HIRExpressionStatement>(span, initialize));
        auto* initialized = program.nodes.create<HIRBooleanLiteral>(span, true, boolean);
        auto* assignment = program.nodes.create<HIRAssignExpression>(span, flag, initialized, boolean);
        statements.push_back(program.nodes.create<HIRExpressionStatement>(span, assignment));
        auto* address = program.nodes.create<HIRUnaryExpression>(span,
            UnaryOperator::Type::AddressOfMut, storage, pointer_type);
        statements.push_back(program.nodes.create<HIRExpressionStatement>(span, address));
        auto* pointer = program.nodes.create<HIRBlockExpression>(span,
            program.nodes.create<HIRBlockStatement>(span, std::move(statements)), pointer_type);

        return program.nodes.create<HIRUnaryExpression>(span, UnaryOperator::Type::Dereference, pointer, type);
    }
};
