module;

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.compiler.lowering.specialization;

import zep.compiler.lowering.mangler;
import zep.frontend.node;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.hir.node;

export class MonoCacheResult {
  public:
    std::string name;
    bool is_generated;

    MonoCacheResult(std::string name, bool is_generated)
        : name(std::move(name)), is_generated(is_generated) {}
};

export class MonomorphizationCache {
  private:
    class SpecializationKey {
      public:
        std::variant<const Node*, const Type*> definition;
        std::vector<GenericBinding> arguments;

        SpecializationKey(std::variant<const Node*, const Type*> definition,
                          std::vector<GenericBinding> arguments)
            : definition(definition), arguments(std::move(arguments)) {}

        bool operator==(const SpecializationKey&) const = default;
    };

    class SpecializationHash {
      public:
        std::size_t operator()(const SpecializationKey& key) const {
            auto result = std::hash<decltype(key.definition)>()(key.definition);
            for (const auto& argument : key.arguments) {
                result ^=
                    GenericBindingHash{}(argument) + 0x9e3779b9 + (result << 6) + (result >> 2);
            }
            return result;
        }
    };

    std::unordered_map<SpecializationKey, std::string, SpecializationHash> specializations;
    std::unordered_map<std::string, const StructDeclaration*> structs;
    std::unordered_map<std::string, const FunctionDeclaration*> functions;
    std::unordered_map<const FunctionSymbol*, const FunctionDeclaration*> function_definitions;
    std::vector<HIRFunctionDeclaration*> pending_specializations;

  public:
    MonomorphizationCache() = default;

    MonomorphizationCache(const MonomorphizationCache&) = delete;
    MonomorphizationCache& operator=(const MonomorphizationCache&) = delete;
    MonomorphizationCache(MonomorphizationCache&&) = delete;
    MonomorphizationCache& operator=(MonomorphizationCache&&) = delete;

    void register_function(const std::string& name, const FunctionDeclaration* statement,
                           const FunctionSymbol* symbol = nullptr) {
        functions[name] = statement;
        if (symbol != nullptr) {
            function_definitions[symbol] = statement;
        }
    }

    void register_struct(const std::string& name, const StructDeclaration* statement) {
        structs[name] = statement;
    }

    bool is_generic_function(const std::string& name) const { return functions.contains(name); }

    bool is_generic_struct(const std::string& name) const { return structs.contains(name); }

    const FunctionDeclaration* get_function(const std::string& name) const {
        const auto iterator = functions.find(name);
        return iterator != functions.end() ? iterator->second : nullptr;
    }

    const FunctionDeclaration* get_function(const FunctionSymbol* symbol) const {
        const auto iterator = function_definitions.find(symbol);
        return iterator != function_definitions.end() ? iterator->second : nullptr;
    }

    const StructDeclaration* get_struct(const std::string& name) const {
        const auto iterator = structs.find(name);
        return iterator != structs.end() ? iterator->second : nullptr;
    }

    void clear_pending_specializations() { pending_specializations.clear(); }

    void enqueue_specialization(HIRFunctionDeclaration* function) {
        pending_specializations.push_back(function);
    }

    void drain_pending_specializations_into(std::vector<HIRFunctionDeclaration*>& destination) {
        for (auto* item : pending_specializations) {
            destination.push_back(item);
        }

        pending_specializations.clear();
    }

    MonoCacheResult get_or_create(std::variant<const Node*, const Type*> definition,
                                  const std::string& name,
                                  const std::vector<GenericBinding>& arguments) {
        SpecializationKey key(definition, arguments);
        if (const auto iterator = specializations.find(key); iterator != specializations.end()) {
            return MonoCacheResult(iterator->second, true);
        }

        auto full = Mangler::mangle(name, arguments);
        specializations.emplace(std::move(key), full);
        return MonoCacheResult(std::move(full), false);
    }

    bool mark_specialization(const Node* definition, const std::vector<GenericBinding>& arguments,
                             const std::string& name) {
        return specializations.emplace(SpecializationKey(definition, arguments), name).second;
    }
};
