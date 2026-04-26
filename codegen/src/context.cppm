module;

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.context;

export class CodegenContext {
  private:
    std::vector<std::unordered_map<std::string, llvm::Value*>> values;

  public:
    llvm::LLVMContext llvm_context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    explicit CodegenContext()
        : builder(llvm_context), module(std::make_unique<llvm::Module>("zep", llvm_context)) {
        enter_scope();
    }

    void enter_scope() { values.emplace_back(); }

    void exit_scope() { values.pop_back(); }

    void set(const std::string& name, llvm::Value* value) { values.back()[name] = value; }

    llvm::Value* lookup(const std::string& name) {
        for (auto& value : std::views::reverse(values)) {
            if (value.contains(name)) {
                return value[name];
            }
        }

        return nullptr;
    }
};
