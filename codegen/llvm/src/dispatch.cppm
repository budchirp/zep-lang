module;

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <string>
#include <vector>

export module zep.codegen.llvm.dispatch;

import zep.frontend.sema.type;
import zep.frontend.sema.scope;

export class DispatchBuilder {
  private:
    llvm::Module& module;
    llvm::IRBuilder<>& builder;

    llvm::Constant* resolve_method_pointer(const StructType* struct_type,
                                           const std::string& method_name) {
        if (struct_type == nullptr) {
            return llvm::ConstantPointerNull::get(builder.getPtrTy());
        }

        if (struct_type->member_scope != nullptr) {
            const auto* scope = static_cast<const Scope*>(struct_type->member_scope);
            const auto* overloads = scope->find_local_function_overloads(method_name);
            if (overloads != nullptr && !overloads->empty()) {
                const auto* symbol = (*overloads)[0];
                if (auto* function = llvm::dyn_cast_or_null<llvm::Function>(
                        module.getNamedValue(symbol->name))) {
                    return llvm::ConstantExpr::getBitCast(function, builder.getPtrTy());
                }
            }
        }

        if (struct_type->base_type != nullptr) {
            return resolve_method_pointer(struct_type->base_type, method_name);
        }

        return llvm::ConstantPointerNull::get(builder.getPtrTy());
    }

  public:
    explicit DispatchBuilder(llvm::Module& module, llvm::IRBuilder<>& builder)
        : module(module), builder(builder) {}

    llvm::GlobalVariable* get(const StructType* struct_type, const InterfaceType* interface_type) {
        if (struct_type == nullptr || interface_type == nullptr) {
            return nullptr;
        }

        auto name = ".method_table." + struct_type->name + "." + interface_type->name;
        if (auto* existing = module.getGlobalVariable(name)) {
            return existing;
        }

        std::vector<llvm::Constant*> entries;
        entries.reserve(interface_type->methods.size());

        for (const auto& method : interface_type->methods) {
            entries.push_back(resolve_method_pointer(struct_type, method.name));
        }

        auto* element_type = builder.getPtrTy();
        auto* array_type = llvm::ArrayType::get(element_type, entries.size());
        auto* initializer = llvm::ConstantArray::get(array_type, entries);
        return new llvm::GlobalVariable(module, array_type, true, llvm::GlobalValue::PrivateLinkage,
                                        initializer, name);
    }
};
