module;

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.llvm.helper;

import zep.codegen.llvm.context;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;
import zep.common.logger;

export class LLVMCodegenHelper {
  private:
    LLVMCodegenContext& context;
    std::unordered_map<std::string, llvm::StructType*> struct_types;

  public:
    explicit LLVMCodegenHelper(LLVMCodegenContext& context) : context(context) {}

    llvm::Type* get_llvm_type(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        if (type->is<IntegerType>()) {
            const auto* integer_type = type->as<IntegerType>();
            return context.builder.getIntNTy(static_cast<unsigned>(integer_type->size));
        }

        if (type->is<FloatType>()) {
            const auto* float_type = type->as<FloatType>();
            switch (float_type->size) {
            case 16:
                return context.builder.getHalfTy();
            case 32:
                return context.builder.getFloatTy();
            case 64:
                return context.builder.getDoubleTy();
            default:
                return context.builder.getFloatTy();
            }
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
            const auto* struct_type = type->as<StructType>();

            auto it = struct_types.find(struct_type->name);
            if (it != struct_types.end()) {
                return it->second;
            }

            auto* llvm_struct = llvm::StructType::create(context.llvm_context, struct_type->name);
            struct_types[struct_type->name] = llvm_struct;

            std::vector<llvm::Type*> field_types;
            field_types.reserve(struct_type->fields.size());

            for (const auto& field : struct_type->fields) {
                field_types.push_back(get_llvm_type(field.type));
            }

            llvm_struct->setBody(field_types);

            return llvm_struct;
        }

        return context.builder.getInt32Ty();
    }

    void declare_types(const Scope& global_scope) {
        for (const auto& [name, symbol] : global_scope.types) {
            if (const auto* struct_type = symbol->type->as<StructType>(); struct_type != nullptr) {
                struct_types[struct_type->name] =
                    llvm::StructType::create(context.llvm_context, struct_type->name);
            }
        }

        for (const auto& [name, symbol] : global_scope.types) {
            if (const auto* struct_type = symbol->type->as<StructType>(); struct_type != nullptr) {
                auto* llvm_struct = struct_types[struct_type->name];

                std::vector<llvm::Type*> llvm_field_types;
                llvm_field_types.reserve(struct_type->fields.size());

                for (const auto& field : struct_type->fields) {
                    auto* field_type = get_llvm_type(field.type);
                    if (field_type == nullptr) {
                        continue;
                    }
                    llvm_field_types.push_back(field_type);
                }

                llvm_struct->setBody(llvm_field_types);
            }
        }
    }

    void declare_functions(const Scope& global_scope) {
        for (const auto& [name, overloads] : global_scope.functions) {
            for (const auto* symbol : overloads) {
                std::vector<llvm::Type*> llvm_parameter_types;
                llvm_parameter_types.reserve(symbol->function_type->parameters.size());

                for (const auto& param : symbol->function_type->parameters) {
                    llvm_parameter_types.push_back(get_llvm_type(param.type));
                }

                auto* llvm_function_type =
                    llvm::FunctionType::get(get_llvm_type(symbol->function_type->return_type),
                                            llvm_parameter_types, symbol->function_type->variadic);

                auto llvm_linkage = symbol->linkage == Linkage::Type::External
                                        ? llvm::Function::ExternalLinkage
                                    : symbol->visibility == Visibility::Type::Public
                                        ? llvm::Function::ExternalLinkage
                                        : llvm::Function::InternalLinkage;

                llvm::Function::Create(llvm_function_type, llvm_linkage, symbol->name,
                                       *context.module);
            }
        }
    }
};
