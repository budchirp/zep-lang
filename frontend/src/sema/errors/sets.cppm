module;

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.types.errors;

import zep.frontend.sema.types;
import zep.frontend.sema.scopes.scope;

export class ErrorSet {
  public:
    static std::string key(const Type* type) {
        if (type == nullptr) {
            return std::string();
        }

        if (const auto* pointer = type->as<PointerType>(); pointer != nullptr) {
            return std::string(pointer->is_mutable ? "pointer/mut/" : "pointer/const/") +
                std::to_string(static_cast<unsigned>(pointer->reference_kind)) + "/" + key(pointer->element);
        }

        if (const auto* array = type->as<ArrayType>(); array != nullptr) {
            return array->to_string() + "/" + key(array->element);
        }

        if (const auto* function = type->as<FunctionType>(); function != nullptr) {
            const auto returned = key(function->return_type);
            auto result = std::string(function->variadic ? "variadic/" : "fn/") +
                std::to_string(returned.size()) + ":" + returned;

            for (const auto& parameter : function->parameters) {
                const auto value = key(parameter.type);
                result += "/" + std::to_string(value.size()) + ":" + value;
            }

            for (const auto* error : function->errors) {
                const auto value = key(error);
                result += "/throws/" + std::to_string(value.size()) + ":" + value;
            }

            return result;
        }

        const auto* nominal = type->as_nominal();

        if (nominal == nullptr) {
            return type->to_string();
        }

        auto result = Scope::qualified_name(*nominal);

        for (const auto& argument : nominal->generic_arguments) {
            auto value = key(argument.type);

            if (argument.is_const()) {
                value = argument.const_binding->value.has_value()
                    ? argument.const_binding->value->to_string() : std::string("dependent");
            }

            result += "/" + std::to_string(value.size()) + ":" + value;
        }

        return result;
    }

    static bool contains(const std::vector<const Type*>& errors, const Type* type) {
        return std::ranges::any_of(errors, [type](const auto* error) { return error->same(type); });
    }

    static std::vector<const Type*> normalize(std::vector<const Type*> errors) {
        std::ranges::sort(errors, {}, [](const auto* type) { return key(type); });
        const auto duplicates = std::ranges::unique(errors,
            [](const auto* left, const auto* right) { return left->same(right); });
        errors.erase(duplicates.begin(), duplicates.end());

        return errors;
    }

    static TypeLayout::Result layout(const std::vector<const Type*>& errors) {
        TypeLayout::Result storage;

        for (const auto* type : errors) {
            const auto payload = TypeLayout().resolve(type);

            if (payload.state != TypeLayout::Result::State::Concrete) {
                return payload;
            }

            storage.size = std::max(storage.size, payload.size);
            storage.alignment = std::max(storage.alignment, payload.alignment);
        }

        storage.append(TypeLayout::Result(0, storage.alignment));

        return storage;
    }
};
