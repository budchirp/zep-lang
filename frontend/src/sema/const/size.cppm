module;

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.frontend.sema.const_size;

import zep.frontend.sema.type;

static constexpr auto maximum_const_size =
    static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());

static bool const_fields_fit(const std::vector<FieldType>& fields, std::size_t& total,
                             std::size_t& alignment, std::unordered_set<const Type*>& active,
                             bool pad_end = true);

static std::optional<std::size_t> const_size_of(const Type* type,
                                                std::unordered_set<const Type*>& active);

static std::optional<std::size_t> const_measure(const Type* type,
                                                std::unordered_set<const Type*>& active) {
    if (const auto* nominal = type->as_nominal();
        nominal != nullptr && !nominal->generic_parameters.empty()) {
        return std::nullopt;
    }

    if (const auto* array = type->as<ArrayType>(); array != nullptr) {
        const auto* concrete = std::get_if<ConcreteArrayExtent>(&array->extent);
        if (concrete == nullptr) {
            return std::nullopt;
        }

        auto element = const_size_of(array->element, active);
        const auto count = concrete->value;
        if (!element.has_value() || (*element != 0 && count > maximum_const_size / *element)) {
            return std::nullopt;
        }

        return *element * count;
    }

    if (const auto* structure = type->as<StructType>(); structure != nullptr) {
        auto total = std::size_t{0};
        auto alignment = std::size_t{1};
        return const_fields_fit(structure->fields, total, alignment, active)
                   ? std::optional<std::size_t>(total)
                   : std::nullopt;
    }

    if (const auto* enumeration = type->as<EnumType>(); enumeration != nullptr) {
        if (enumeration->backing_type != nullptr) {
            return const_size_of(enumeration->backing_type, active);
        }

        auto total = std::size_t{4};
        auto alignment = std::size_t{4};
        for (const auto& variant : enumeration->variants) {
            if (!const_fields_fit(variant.fields, total, alignment, active, false)) {
                return std::nullopt;
            }
        }

        total = Type::align_to(total, alignment);
        return total <= maximum_const_size ? std::optional<std::size_t>(total) : std::nullopt;
    }

    return type->byte_size();
}

static std::optional<std::size_t> const_size_of(const Type* type,
                                                std::unordered_set<const Type*>& active) {
    if (type == nullptr || type->is<NamedType>() || type->is<VoidType>() || type->is<NeverType>() ||
        !active.insert(type).second) {
        return std::nullopt;
    }

    auto result = const_measure(type, active);
    active.erase(type);
    return result;
}

static bool const_fields_fit(const std::vector<FieldType>& fields, std::size_t& total,
                             std::size_t& alignment, std::unordered_set<const Type*>& active,
                             bool pad_end) {
    for (const auto& field : fields) {
        auto size = const_size_of(field.type, active);
        if (!size.has_value()) {
            return false;
        }

        const auto field_alignment = field.type->alignment();
        total = Type::align_to(total, field_alignment);
        if (total > maximum_const_size || *size > maximum_const_size - total) {
            return false;
        }

        total += *size;
        alignment = alignment > field_alignment ? alignment : field_alignment;
    }

    if (pad_end) {
        total = Type::align_to(total, alignment);
    }
    return total <= maximum_const_size;
}

export std::optional<std::size_t> compile_time_size_of(const Type* type) {
    std::unordered_set<const Type*> active;
    return const_size_of(type, active);
}
