module;

#include <cstddef>
#include <string>
#include <utility>

export module zep.frontend.sema.resolution.facades;

import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.types.facades;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.generics.environment;
import zep.frontend.sema.scopes.scope;

export class Facades {
  private:
    FacadeRegistry& registry;
    TypeResolver& resolver;

    const FacadeEntry* lookup(const Type* type) const {
        if (const auto* entry = registry.find(type); entry != nullptr) {
            return entry;
        }

        for (const auto& [definition, entry] : registry.entries) {
            if (!entry.parameter.has_value()) {
                continue;
            }

            const auto* constraint = definition->generic_parameters[*entry.parameter].type;

            if (constraint != nullptr && resolver.satisfies_constraint(type, constraint)) {
                return &entry;
            }
        }

        return nullptr;
    }

  public:
    Facades(FacadeRegistry& registry, TypeResolver& resolver)
        : registry(registry), resolver(resolver) {}

    void add(const StructType& type, Visibility::Type visibility) {
        if (visibility != Visibility::Type::Public) {
            return;
        }

        const Type* backing = nullptr;

        for (const auto* interface : type.interfaces) {
            if (interface != nullptr && interface->name == "Primitive" &&
                interface->generic_arguments.size() == 1) {
                backing = interface->generic_arguments.front().type;
                break;
            }
        }

        const auto* field = type.find_field("value");

        if (backing == nullptr || field == nullptr || field->type == nullptr) {
            return;
        }

        const auto* parameter = backing->as<NamedType>();
        const auto* value = field->type->as<NamedType>();

        if (parameter != nullptr && value != nullptr && parameter->same(value)) {
            for (auto index = std::size_t{0}; index < type.generic_parameters.size(); ++index) {
                const auto& candidate = type.generic_parameters[index];
                const auto matches = parameter->declaration != nullptr
                    ? parameter->declaration == candidate.declaration : parameter->name == candidate.name;

                if (matches) {
                    registry.add(FacadeEntry(&type, nullptr, index));
                    return;
                }
            }
        }

        const auto* resolved = resolver.resolve_type(backing);
        const auto* stored = resolver.resolve_type(field->type);

        if (resolved != nullptr && stored != nullptr && stored->accepts(resolved)) {
            registry.add(FacadeEntry(&type, resolved));
        }
    }

    const StructType* wrapper(const Type* type) const {
        const auto* resolved = resolver.resolve_type(type);

        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto* structure = resolved->as<StructType>();
            structure != nullptr && registry.find(*structure) != nullptr) {
            return structure;
        }

        const auto* entry = lookup(resolved);

        if (entry == nullptr) {
            return nullptr;
        }

        if (!entry->parameter.has_value()) {
            return entry->type;
        }

        const auto& parameter = entry->type->generic_parameters[*entry->parameter];
        CompileTimeEnvironment environment;
        environment.bind(parameter, TypeBinding(resolved));

        return resolver.instantiate_struct(*entry->type, environment);
    }

    const Type* backing(const Type* type) const {
        const auto* resolved = resolver.resolve_type(type);
        const auto* structure = resolved != nullptr ? resolved->as<StructType>() : nullptr;
        const auto* entry = structure != nullptr ? registry.find(*structure) : nullptr;

        if (entry == nullptr) {
            return nullptr;
        }

        if (!entry->parameter.has_value()) {
            return entry->backing;
        }

        const auto index = *entry->parameter;

        return index < structure->generic_arguments.size()
            ? resolver.resolve_type(structure->generic_arguments[index].type) : nullptr;
    }

    Scope* scope(const Type* type) const {
        return Scope::members(wrapper(type));
    }

    const Type* value(const Type* type) const {
        if (const auto* resolved = backing(type); resolved != nullptr) {
            return resolved;
        }

        return wrapper(type) != nullptr ? resolver.resolve_type(type) : nullptr;
    }
};
