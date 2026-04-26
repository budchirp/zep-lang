module;

#include <llvm/IR/Value.h>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.scope;

export class CodegenScope {
  private:
    std::vector<std::unordered_map<std::string, llvm::Value*>> values;

  public:
    CodegenScope() { values.emplace_back(); }

    void enter() { values.emplace_back(); }

    void exit() { values.pop_back(); }

    void set(const std::string& name, llvm::Value* value) { values.back()[name] = value; }

    llvm::Value* lookup(const std::string& name) {
        for (auto& frame : std::views::reverse(values)) {
            if (frame.contains(name)) {
                return frame[name];
            }
        }

        return nullptr;
    }
};
