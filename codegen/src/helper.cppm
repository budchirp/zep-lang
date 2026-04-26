module;

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <string>
#include <vector>

export module zep.codegen.helper;

import zep.codegen.context;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;

export class CodegenHelper {
  private:
    CodegenContext& context;

  public:
    explicit CodegenHelper(CodegenContext& context) : context(context) {}

    llvm::Type* get_llvm_type(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        if (type->is<IntegerType>()) {
            auto* integer_type = type->as<IntegerType>();
            return context.builder.getIntNTy(static_cast<unsigned>(integer_type->size));
        }

        if (type->is<FloatType>()) {
            auto* float_type = type->as<FloatType>();
            if (float_type->size == 16) return context.builder.getHalfTy();
            if (float_type->size == 32) return context.builder.getFloatTy();
            if (float_type->size == 64) return context.builder.getDoubleTy();
            return context.builder.getFloatTy();
        }

        if (type->is<BooleanType>()) {
            return context.builder.getInt1Ty();
        }

        if (type->is<VoidType>()) {
            return context.builder.getVoidTy();
        }

        if (type->is<PointerType>() || type->is<StringType>()) {
            return context.builder.getPtrTy();
        }

        if (type->is<StructType>()) {
            auto* struct_type = type->as<StructType>();
            auto* llvm_type = llvm::StructType::getTypeByName(context.llvm_context, struct_type->name);
            if (llvm_type != nullptr) {
                return llvm_type;
            }
        }

        return context.builder.getInt32Ty();
    }

    void declare_types(const Scope& global_scope) {
        // Pass 1: Create opaque types
        for (const auto& [name, symbol] : global_scope.types) {
            if (auto* struct_type = symbol->type->as<StructType>(); struct_type != nullptr) {
                if (llvm::StructType::getTypeByName(context.llvm_context, struct_type->name) ==
                    nullptr) {
                    llvm::StructType::create(context.llvm_context, struct_type->name);
                }
            }
        }

        // Pass 2: Set bodies
        for (const auto& [name, symbol] : global_scope.types) {
            if (auto* struct_type = symbol->type->as<StructType>(); struct_type != nullptr) {
                auto* llvm_structure =
                    llvm::StructType::getTypeByName(context.llvm_context, struct_type->name);
                if (llvm_structure != nullptr && llvm_structure->isOpaque()) {
                    std::vector<llvm::Type*> field_types;
                    for (const auto& field : struct_type->fields) {
                        field_types.push_back(get_llvm_type(field.type));
                    }
                    llvm_structure->setBody(field_types);
                }
            }
        }
    }

    void declare_functions(const Scope& global_scope) {
        for (const auto& [name, overloads] : global_scope.functions) {
            for (const auto* symbol : overloads) {
                auto* function_type = symbol->type->as<FunctionType>();
                if (function_type == nullptr) {
                    continue;
                }

                std::vector<llvm::Type*> parameter_types;
                for (const auto& param : function_type->parameters) {
                    parameter_types.push_back(get_llvm_type(param.type));
                }

                auto* llvm_function_type = llvm::FunctionType::get(
                    get_llvm_type(function_type->return_type), parameter_types, function_type->variadic);

                auto llvm_linkage = symbol->linkage == Linkage::Type::External
                                        ? llvm::Function::ExternalLinkage
                                        : llvm::Function::InternalLinkage;

                llvm::Function::Create(llvm_function_type, llvm_linkage, symbol->name,
                                       *context.module);
            }
        }
    }
};
