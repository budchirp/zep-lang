module;

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.generics.environment;

import zep.frontend.sema.types;

export class CompileTimeEnvironment {
  private:
    class Frame {
      public:
        std::unordered_map<std::string, std::optional<GenericBinding>> names;
        std::unordered_map<const void*, std::optional<GenericBinding>> bindings;
    };

    std::vector<Frame> frames;

    template <typename Key>
    void record(const std::unordered_map<Key, GenericBinding>& values,
                std::unordered_map<Key, std::optional<GenericBinding>>& previous,
                const Key& key) {
        if (previous.contains(key)) {
            return;
        }

        const auto iterator = values.find(key);
        previous.emplace(key, iterator != values.end()
                                  ? std::optional<GenericBinding>(iterator->second)
                                  : std::nullopt);
    }

    template <typename Key>
    void restore(std::unordered_map<Key, GenericBinding>& values,
                 std::unordered_map<Key, std::optional<GenericBinding>>& previous) {
        for (auto& [key, value] : previous) {
            if (value.has_value()) {
                values.insert_or_assign(std::move(key), std::move(*value));
            } else {
                values.erase(key);
            }
        }
    }

    void leave_scope() {
        auto frame = std::move(frames.back());
        frames.pop_back();

        restore(names, frame.names);
        restore(bindings, frame.bindings);
    }

  public:
    class Scope {
      private:
        CompileTimeEnvironment& environment;

      public:
        explicit Scope(CompileTimeEnvironment& environment) : environment(environment) {
            environment.frames.emplace_back();
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        ~Scope() { environment.leave_scope(); }
    };

    std::unordered_map<std::string, GenericBinding> names;
    std::unordered_map<const void*, GenericBinding> bindings;

    void bind(const std::string& name, GenericBinding binding, const void* declaration = nullptr) {
        if (!frames.empty()) {
            record(names, frames.back().names, name);
        }
        names.insert_or_assign(name, binding);

        if (declaration != nullptr) {
            if (!frames.empty()) {
                record(bindings, frames.back().bindings, declaration);
            }
            bindings.insert_or_assign(declaration, std::move(binding));
        }
    }

    void bind(const GenericParameterType& parameter, GenericBinding binding) {
        bind(parameter.name, std::move(binding), parameter.declaration);
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

    bool contains(const GenericParameterType& parameter) const {
        return parameter.declaration != nullptr ? bindings.contains(parameter.declaration)
                                                : names.contains(parameter.name);
    }

    bool empty() const {
        return names.empty();
    }

    [[nodiscard]] Scope enter_scope() {
        return Scope(*this);
    }
};
