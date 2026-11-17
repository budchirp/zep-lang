module;

#include <cstdint>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.frontend.sema.checking.errors;

import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.types.errors;
import zep.frontend.sema.types.resolver;

export class Errors {
  private:
    bool valid(const Type* type, Span span, TypeResolver& resolver, Diagnostics& diagnostics) const {
        const auto* permitted = type != nullptr ? resolver.constraint_type(type) : nullptr;
        const auto* structure = permitted != nullptr ? permitted->as<StructType>() : nullptr;
        const auto* interface = permitted != nullptr ? permitted->as<InterfaceType>() : nullptr;

        if (throwable != nullptr && structure != nullptr && structure->implements(throwable)) {
            return true;
        }

        if (type != nullptr && type->is<NamedType>() && interface != nullptr &&
            throwable != nullptr &&
            (interface->same(throwable) || interface->inherits_from(throwable))) {
            return true;
        }

        diagnostics.add_error(span, "error types must be structs implementing std.core.error.Throwable");

        return false;
    }

  public:
    class Target {
      public:
        const Type* type;
        const CatchArm* handler;

        Target(const Type* type, const CatchArm* handler) : type(type), handler(handler) {}
    };

    static void check_foreign(Diagnostics& diagnostics, const FunctionType& signature, Span span) {
        std::vector<const Type*> pending;
        pending.reserve(16);
        pending.push_back(&signature);
        std::unordered_set<const Type*> visited;
        visited.reserve(16);

        while (!pending.empty()) {
            const auto* type = pending.back();
            pending.pop_back();

            if (type == nullptr || !visited.insert(type).second) {
                continue;
            }

            if (const auto* function = type->as<FunctionType>(); function != nullptr) {
                if (!function->errors.empty()) {
                    diagnostics.add_error(span, type == &signature
                        ? "C ABI functions cannot propagate checked errors"
                        : "C ABI callbacks cannot propagate checked errors");

                    return;
                }

                pending.push_back(function->return_type);

                for (const auto& parameter : function->parameters) {
                    pending.push_back(parameter.type);
                }
            } else if (const auto* pointer = type->as<PointerType>(); pointer != nullptr) {
                pending.push_back(pointer->element);
            } else if (const auto* array = type->as<ArrayType>(); array != nullptr) {
                pending.push_back(array->element);
            } else if (const auto* structure = type->as<StructType>(); structure != nullptr) {
                for (const auto& field : structure->fields) {
                    pending.push_back(field.type);
                }
            } else if (const auto* enumeration = type->as<EnumType>(); enumeration != nullptr) {
                for (const auto& field : enumeration->fields) {
                    pending.push_back(field.type);
                }

                for (const auto& variant : enumeration->variants) {
                    for (const auto& field : variant.fields) {
                        pending.push_back(field.type);
                    }
                }
            } else if (const auto* interface = type->as<InterfaceType>(); interface != nullptr) {
                for (const auto& method : interface->methods) {
                    pending.push_back(method.type);
                }
            }
        }
    }

    class Origin {
      public:
        enum class Type : std::uint8_t { Call, Forward, Throw };
    };

    class Boundary {
      public:
        CatchExpression* handler;
        std::vector<const Type*> errors;

        explicit Boundary(CatchExpression* handler) : handler(handler), errors() {}
    };

    class State {
      public:
        std::vector<const Type*> declared;
        std::vector<const Type*> escaped;
        std::vector<Boundary> boundaries;
        bool inferred;

        explicit State(std::vector<const Type*> declared = {}, bool inferred = false)
            : declared(std::move(declared)), escaped(), boundaries(), inferred(inferred) {}
    };

    State state;
    const InterfaceType* throwable;
    std::unordered_map<const Expression*, std::vector<Target>> targets;

    Errors() : state(), throwable(nullptr), targets() {}

    std::vector<const Type*> declarations(const std::vector<TypeExpression*>& annotations,
                                         Visitor<void>& visitor, TypeResolver& resolver,
                                         Diagnostics& diagnostics) const {
        std::vector<const Type*> result;
        result.reserve(annotations.size());

        for (auto* annotation : annotations) {
            visitor.visit(*annotation);
            const auto* type = resolver.resolve_type(annotation->type);

            if (!valid(type, annotation->span, resolver, diagnostics)) {
                continue;
            }

            if (ErrorSet::contains(result, type)) {
                diagnostics.add_error(annotation->span, "duplicate declared error type '" + type->to_string() + "'");
                continue;
            }

            result.push_back(type);
        }

        return ErrorSet::normalize(std::move(result));
    }

    void protect(CatchExpression* handler = nullptr) {
        state.boundaries.emplace_back(handler);
    }

    std::vector<const Type*> finish() {
        auto errors = std::move(state.boundaries.back().errors);
        state.boundaries.pop_back();

        return ErrorSet::normalize(std::move(errors));
    }

    void raise(Expression& expression, const Type* type, Origin::Type origin,
               TypeResolver& resolver, Diagnostics& diagnostics) {
        if (type == nullptr || !valid(type, expression.span, resolver, diagnostics)) {
            return;
        }

        const CatchArm* handler = nullptr;

        for (const auto& boundary : state.boundaries | std::views::reverse) {
            if (boundary.handler == nullptr) {
                continue;
            }

            for (const auto& arm : boundary.handler->arms) {
                if (arm.annotation->type != nullptr && arm.annotation->type->same(type)) {
                    handler = &arm;
                    break;
                }
            }

            if (handler != nullptr) {
                break;
            }
        }

        targets[&expression].emplace_back(type, handler);

        if (!state.boundaries.empty()) {
            auto& errors = state.boundaries.back().errors;

            if (!ErrorSet::contains(errors, type)) {
                errors.push_back(type);
            }

            return;
        }

        if (origin == Origin::Type::Call) {
            diagnostics.add_error(expression.span, "fallible expression requires try or catch");
            return;
        }

        if (!state.inferred && !ErrorSet::contains(state.declared, type)) {
            diagnostics.add_error(expression.span, "undeclared error type '" + type->to_string() + "'");
            return;
        }

        if (!ErrorSet::contains(state.escaped, type)) {
            state.escaped.push_back(type);
        }
    }
};
