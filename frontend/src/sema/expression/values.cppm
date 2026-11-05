module;

#include <string>

export module zep.frontend.sema.expression.values;

import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.scopes.environment;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;

export class Values {
  public:
    static bool moving(const Expression& expression) {
        if (const auto* coercion = expression.as<CoerceExpression>(); coercion != nullptr) {
            return coercion->coercion == Coercion::Type::MoveValue;
        }

        const Type* type = nullptr;

        if (const auto* call = expression.as<CallExpression>(); call != nullptr) {
            type = call->reference_result;
        } else if (const auto* identifier = expression.as<IdentifierExpression>();
                   identifier != nullptr && identifier->var_symbol != nullptr) {
            type = identifier->var_symbol->type;
        }

        const auto* reference = type != nullptr ? type->as<PointerType>() : nullptr;

        return reference != nullptr &&
               reference->reference_kind == PointerType::ReferenceKind::Type::Move;
    }

    static bool place(const Expression& expression) {
        if (const auto* coercion = expression.as<CoerceExpression>(); coercion != nullptr) {
            return coercion->coercion == Coercion::Type::BaseSlice;
        }

        if (expression.as<IdentifierExpression>() != nullptr ||
            expression.as<MemberExpression>() != nullptr ||
            expression.as<IndexExpression>() != nullptr) {
            return true;
        }

        if (const auto* call = expression.as<CallExpression>(); call != nullptr) {
            return call->reference_result != nullptr;
        }

        const auto* unary = expression.as<UnaryExpression>();

        return unary != nullptr && unary->op == UnaryOperator::Type::Dereference;
    }

    static bool mutable_place(const Expression& expression, const SymbolEnvironment& environment) {
        if (const auto* call = expression.as<CallExpression>(); call != nullptr &&
            call->reference_result != nullptr) {
            const auto* reference = call->reference_result->as<PointerType>();

            return reference != nullptr && reference->is_mutable;
        }

        if (const auto* identifier = expression.as<IdentifierExpression>(); identifier != nullptr) {
            if (identifier->implicit_self_field) {
                const auto* self = environment.current_scope->lookup_var("self");
                const auto* pointer = self != nullptr && self->type != nullptr
                    ? self->type->as<PointerType>() : nullptr;

                return pointer != nullptr && pointer->is_mutable;
            }

            const auto* symbol = identifier->var_symbol != nullptr ? identifier->var_symbol
                : environment.current_scope->lookup_var(identifier->name);
            const auto* reference = symbol != nullptr ? symbol->type->as<PointerType>() : nullptr;

            if (reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
                return reference->is_mutable;
            }

            return symbol != nullptr && symbol->storage_kind == StorageKind::Type::VarMut;
        }

        if (const auto* unary = expression.as<UnaryExpression>(); unary != nullptr &&
            unary->op == UnaryOperator::Type::Dereference) {
            const auto* pointer = unary->operand->type != nullptr
                ? unary->operand->type->as<PointerType>() : nullptr;

            return pointer != nullptr && pointer->is_mutable;
        }

        if (const auto* member = expression.as<MemberExpression>(); member != nullptr) {
            const auto* structure = member->value->type != nullptr
                ? member->value->type->as<StructType>() : nullptr;
            const auto* field = structure != nullptr ? structure->find_field(member->member) : nullptr;
            const auto* reference = field != nullptr ? field->type->as<PointerType>() : nullptr;

            if (reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
                return reference->is_mutable;
            }

            return mutable_place(*member->value, environment);
        }

        if (const auto* index = expression.as<IndexExpression>(); index != nullptr) {
            const auto* pointer = index->value->type != nullptr ? index->value->type->as<PointerType>() : nullptr;

            return pointer != nullptr ? pointer->is_mutable : mutable_place(*index->value, environment);
        }

        return false;
    }
};
