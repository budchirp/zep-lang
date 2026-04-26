module;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>

export module zep.codegen.llvm.context;

export class LLVMCodegenContext {
  public:
    llvm::LLVMContext llvm_context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    explicit LLVMCodegenContext()
        : builder(llvm_context),
          module(std::make_unique<llvm::Module>("zep", llvm_context)) {}
};
