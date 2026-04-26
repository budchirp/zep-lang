module;

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module zep.hir.monomorphizer;

import zep.frontend.sema.type;
import zep.frontend.node;
import zep.frontend.sema.mangler;
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
    std::unordered_set<std::string> specializations;
    std::unordered_map<std::string, const StructDeclaration*> structs;
    std::unordered_map<std::string, const FunctionDeclaration*> functions;
    std::vector<HIRFunctionDeclaration*> pending_specializations;

  public:
    void register_function(const std::string& name, const FunctionDeclaration* statement) {
        functions[name] = statement;
    }

    void register_struct(const std::string& name, const StructDeclaration* statement) {
        structs[name] = statement;
    }

    bool is_generic_function(const std::string& name) const {
        return functions.find(name) != functions.end();
    }

    bool is_generic_struct(const std::string& name) const {
        return structs.find(name) != structs.end();
    }

    const FunctionDeclaration* get_function(const std::string& name) const {
        auto iterator = functions.find(name);
        return iterator != functions.end() ? iterator->second : nullptr;
    }

    const StructDeclaration* get_struct(const std::string& name) const {
        auto iterator = structs.find(name);
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

    MonoCacheResult get_or_create(const std::string& name, const std::vector<const Type*>& types) {
        std::string full = NameMangler::mangle(name, types);

        if (specializations.contains(full)) {
            return MonoCacheResult(full, true);
        }

        specializations.insert(full);
        return MonoCacheResult(std::move(full), false);
    }
};
