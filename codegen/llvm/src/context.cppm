module;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <memory>

export module zep.codegen.llvm.context;

export class LLVMCodegenContext {
  public:
    std::unique_ptr<llvm::LLVMContext> llvm_context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::TargetMachine> target_machine;

    LLVMCodegenContext()
        : llvm_context(std::make_unique<llvm::LLVMContext>()),
          builder(std::make_unique<llvm::IRBuilder<>>(*llvm_context)),
          module(std::make_unique<llvm::Module>("zep", *llvm_context)) {}
};
