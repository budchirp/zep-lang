module;

#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

export module zep.frontend.sema.constant.environment;

import zep.frontend.sema.type;

export class CompileTimeEnvironment {
  private:
    std::unordered_map<std::string, GenericBinding> names;
    std::unordered_map<std::string, const void*> declarations;
    std::unordered_map<const void*, GenericBinding> bindings;

  public:
    void bind(const std::string& name, GenericBinding binding, const void* declaration = nullptr) {
        names.insert_or_assign(name, binding);

        if (declaration != nullptr) {
            declarations.insert_or_assign(name, declaration);
            bindings.insert_or_assign(declaration, std::move(binding));
        }
    }

    void bind(const GenericParameterType& parameter, GenericBinding binding) {
        bind(parameter.name, std::move(binding), parameter.declaration);
    }

    const void* declaration(const std::string& name) const {
        const auto iterator = declarations.find(name);
        return iterator != declarations.end() ? iterator->second : nullptr;
    }

    const GenericBinding* lookup(const std::string& name) const {
        const auto iterator = names.find(name);
        return iterator != names.end() ? &iterator->second : nullptr;
    }

    const GenericBinding* lookup(const void* declaration) const {
        const auto iterator = bindings.find(declaration);
        return iterator != bindings.end() ? &iterator->second : nullptr;
    }

    const GenericBinding* lookup(const GenericParameterType& parameter) const {
        return parameter.declaration != nullptr ? lookup(parameter.declaration)
                                                : lookup(parameter.name);
    }

    const Type* lookup_type(const std::string& name, const void* declaration = nullptr) const {
        const auto* binding = declaration != nullptr ? lookup(declaration) : lookup(name);
        const auto* type = binding != nullptr ? std::get_if<TypeBinding>(binding) : nullptr;
        return type != nullptr ? type->type : nullptr;
    }

    bool contains(const std::string& name) const { return names.contains(name); }

    bool contains(const GenericParameterType& parameter) const {
        return parameter.declaration != nullptr ? bindings.contains(parameter.declaration)
                                                : names.contains(parameter.name);
    }

    bool empty() const { return names.empty(); }

    const std::unordered_map<std::string, GenericBinding>& entries() const { return names; }
};
