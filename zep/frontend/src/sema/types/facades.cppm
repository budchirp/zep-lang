module;

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <utility>

export module zep.frontend.sema.types.facades;

import zep.frontend.sema.types;

export class FacadeRegistry {
  private:
    std::unordered_map<const Type*, const StructType*> backings;

  public:
    class Entry {
      public:
        const StructType* type;
        const Type* backing;
        std::optional<std::size_t> parameter;

        Entry(const StructType* type, const Type* backing,
              std::optional<std::size_t> parameter = std::nullopt)
            : type(type), backing(backing), parameter(parameter) {}
    };

    std::unordered_map<const StructType*, Entry> entries;

    FacadeRegistry() = default;

    static const StructType* identity(const StructType& type) {
        const auto* definition = type.definition != nullptr
            ? type.definition->as<StructType>() : nullptr;

        return definition != nullptr ? definition : &type;
    }

    void add(Entry entry) {
        const auto* type = identity(*entry.type);

        if (entry.backing != nullptr) {
            backings.insert_or_assign(entry.backing, type);
        }

        entries.insert_or_assign(type, std::move(entry));
    }

    const Entry* find(const StructType& type) const {
        const auto found = entries.find(identity(type));

        return found != entries.end() ? &found->second : nullptr;
    }

    const Entry* find(const Type* backing) const {
        const auto found = backings.find(backing);

        return found != backings.end() ? find(*found->second) : nullptr;
    }
};
