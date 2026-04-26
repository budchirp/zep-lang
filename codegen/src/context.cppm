module;

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.context;

export class CodegenContext {
  private:
    std::vector<std::unordered_map<std::string, llvm::Value*>> scopes;

  public:
    llvm::LLVMContext llvm_context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    explicit CodegenContext()
        : builder(llvm_context), module(std::make_unique<llvm::Module>("zep", llvm_context)) {
        enter_scope();
    }

    void enter_scope() { scopes.emplace_back(); }

    void exit_scope() { scopes.pop_back(); }

    void define_local(const std::string& name, llvm::Value* value) { scopes.back()[name] = value; }

    llvm::Value* lookup_local(const std::string& name) {
        for (auto iterator = scopes.rbegin(); iterator != scopes.rend(); ++iterator) {
            if (iterator->contains(name)) {
                return (*iterator)[name];
            }
        }
        return nullptr;
    }
};
