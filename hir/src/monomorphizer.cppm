module;

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module zep.hir.monomorphizer;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.frontend.sema.mangler;
import zep.hir.ir;

export class MonoCacheResult {
  public:
    std::string name;
    bool is_generated;

    MonoCacheResult(std::string name_in, bool is_generated_in)
        : name(std::move(name_in)), is_generated(is_generated_in) {}
};

export class MonomorphizationCache {
  private:
    std::unordered_set<std::string> specializations;
    std::unordered_map<std::string, StructDeclaration*> structs;
    std::unordered_map<std::string, FunctionDeclaration*> functions;
    std::vector<std::unique_ptr<HIRFunctionDeclaration>> pending_specializations;

  public:
    void register_function(const std::string& name, FunctionDeclaration* statement) {
        functions[name] = statement;
    }

    void register_struct(const std::string& name, StructDeclaration* statement) {
        structs[name] = statement;
    }

    bool is_generic_function(const std::string& name) const {
        return functions.find(name) != functions.end();
    }

    bool is_generic_struct(const std::string& name) const {
        return structs.find(name) != structs.end();
    }

    FunctionDeclaration* get_function(const std::string& name) const {
        auto iterator = functions.find(name);
        return iterator != functions.end() ? iterator->second : nullptr;
    }

    StructDeclaration* get_struct(const std::string& name) const {
        auto iterator = structs.find(name);
        return iterator != structs.end() ? iterator->second : nullptr;
    }

    void clear_pending_specializations() { pending_specializations.clear(); }

    void enqueue_specialization(std::unique_ptr<HIRFunctionDeclaration> function) {
        pending_specializations.push_back(std::move(function));
    }

    void drain_pending_specializations_into(
        std::vector<std::unique_ptr<HIRFunctionDeclaration>>& destination) {
        for (auto& item : pending_specializations) {
            destination.push_back(std::move(item));
        }
        pending_specializations.clear();
    }

    MonoCacheResult get_or_create(const std::string& name,
                                  const std::vector<std::shared_ptr<Type>>& types) {
        std::string full = NameMangler::mangle(name, types);
        if (specializations.contains(full)) {
            return MonoCacheResult(full, true);
        }
        specializations.insert(full);
        return MonoCacheResult(std::move(full), false);
    }
};
