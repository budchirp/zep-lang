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
import zep.frontend.sema.symbols;
import zep.frontend.sema.types;
import zep.hir.node;
import zep.common.hash;

export class SpecializationResult {
  public:
    std::string name;
    bool is_generated;

    SpecializationResult(std::string name, bool is_generated)
        : name(std::move(name)), is_generated(is_generated) {}
};

export class Specializations {
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
                result = Hash::combine(result, GenericBindingHash{}(argument));
            }
            return result;
        }
    };

    std::unordered_map<SpecializationKey, std::string, SpecializationHash> specializations;
    std::unordered_map<std::string, const StructDeclaration*> structs;
    std::unordered_map<std::string, const FunctionDeclaration*> functions;
    std::unordered_map<const FunctionSymbol*, const FunctionDeclaration*> function_definitions;
    std::vector<HIRFunctionDeclaration*> pending;

  public:
    Specializations() = default;

    Specializations(const Specializations&) = delete;
    Specializations& operator=(const Specializations&) = delete;
    Specializations(Specializations&&) = delete;
    Specializations& operator=(Specializations&&) = delete;

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

    void enqueue(HIRFunctionDeclaration* function) {
        pending.push_back(function);
    }

    void drain(std::vector<HIRFunctionDeclaration*>& destination) {
        destination.insert(destination.end(), pending.begin(), pending.end());
        pending.clear();
    }

    SpecializationResult get_or_create(std::variant<const Node*, const Type*> definition,
                                  const std::string& name,
                                  const std::vector<GenericBinding>& arguments) {
        SpecializationKey key(definition, arguments);
        if (const auto iterator = specializations.find(key); iterator != specializations.end()) {
            return SpecializationResult(iterator->second, true);
        }

        auto base = name;

        if (const auto* node = std::get_if<const Node*>(&definition); node != nullptr) {
            if (const auto* function = (*node)->as<FunctionDeclaration>(); function != nullptr &&
                function->function_symbol != nullptr) {
                const auto* symbol = function->function_symbol;
                base = Mangler::function_name(symbol->base_name(), symbol, symbol->signature());
            }
        }

        auto full = Mangler::mangle(base, arguments);
        specializations.emplace(std::move(key), full);
        return SpecializationResult(std::move(full), false);
    }

    SpecializationResult lifecycle(const FunctionSymbol& function, const NominalType& owner,
                               const FunctionType* signature) {
        const auto* declaration = get_function(&function);

        if (owner.generic_arguments.empty() || declaration == nullptr) {
            return SpecializationResult(Mangler::function_name(function.base_name(), &function, signature), true);
        }

        std::vector<GenericBinding> arguments;
        arguments.reserve(owner.generic_arguments.size());

        for (const auto& argument : owner.generic_arguments) {
            arguments.push_back(argument.binding());
        }

        return get_or_create(declaration, function.base_name(), arguments);
    }
};
