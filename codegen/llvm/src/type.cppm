module;

#include <cassert>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.codegen.llvm.type;

import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.kind;
import zep.common.logger;
import zep.hir.node;

export class TypeLowerer {
  private:
    llvm::LLVMContext& context;

    std::unordered_map<std::string, llvm::StructType*> type_cache;

    static llvm::GlobalValue::LinkageTypes llvm_linkage(Linkage::Type linkage) {
        switch (linkage) {
        case Linkage::Type::External:
            return llvm::GlobalValue::ExternalLinkage;
        case Linkage::Type::Internal:
            return llvm::GlobalValue::InternalLinkage;
        case Linkage::Type::LinkOnceODR:
            return llvm::GlobalValue::LinkOnceODRLinkage;
        }

        return llvm::GlobalValue::ExternalLinkage;
    }

    std::string llvm_type_name_for(const Type* type) const {
        const auto* nominal = type->as_nominal();
        if (nominal != nullptr) {
            return nominal->name;
        }

        return "";
    }

    static bool is_c_abi_integer_field_type(const Type* type) {
        return type != nullptr && (type->is<IntegerType>() || type->is<CharType>());
    }

    static std::optional<unsigned> c_abi_integer_struct_bits(const Type* type) {
        const auto* struct_type = type != nullptr ? type->as<StructType>() : nullptr;
        if (struct_type == nullptr || struct_type->fields.empty()) {
            return std::nullopt;
        }

        std::size_t total = 0;
        for (const auto& field : struct_type->fields) {
            if (!is_c_abi_integer_field_type(field.type)) {
                return std::nullopt;
            }

            total += field.type->byte_size();
        }

        const auto size = struct_type->byte_size();
        if (total != size) {
            return std::nullopt;
        }

        if (size == 1 || size == 2 || size == 4 || size == 8) {
            return static_cast<unsigned>(size * 8);
        }

        return std::nullopt;
    }

    void populate_struct_body(llvm::StructType* llvm_struct, const StructType* struct_type) {
        std::vector<llvm::Type*> llvm_field_types;
        llvm_field_types.reserve(struct_type->fields.size());

        for (const auto& field : struct_type->fields) {
            auto* field_type = lower(field.type);
            if (field_type == nullptr) {
                return;
            }
            llvm_field_types.push_back(field_type);
        }

        llvm_struct->setBody(llvm_field_types);
    }

    void populate_interface_body(llvm::StructType* llvm_interface) {
        std::vector<llvm::Type*> llvm_field_types;
        llvm_field_types.reserve(2);
        llvm_field_types.push_back(llvm::PointerType::getUnqual(context));
        llvm_field_types.push_back(llvm::PointerType::getUnqual(context));

        llvm_interface->setBody(llvm_field_types);
    }

    void populate_enum_body(llvm::StructType* llvm_enum, const EnumType* enum_type) {
        if (enum_type->backing_type != nullptr) {
            return;
        }

        std::vector<llvm::Type*> llvm_field_types;
        llvm_field_types.reserve(enum_type->byte_size());

        llvm_field_types.push_back(llvm::Type::getInt32Ty(context));

        for (const auto& variant : enum_type->variants) {
            for (const auto& field : variant.fields) {
                auto* field_type = lower(field.type);
                if (field_type == nullptr) {
                    continue;
                }
                llvm_field_types.push_back(field_type);
            }
        }

        llvm_enum->setBody(llvm_field_types);
    }

    void populate_type_body(llvm::StructType* llvm_struct, const Type* type) {
        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            populate_struct_body(llvm_struct, struct_type);
        } else if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            populate_enum_body(llvm_struct, enum_type);
        } else if (type->is<InterfaceType>()) {
            populate_interface_body(llvm_struct);
        }
    }

    static bool has_unresolved_type(const Type* type, std::unordered_set<const Type*>& visited) {
        if (type == nullptr || !visited.insert(type).second) {
            return false;
        }

        if (const auto* named = type->as<NamedType>(); named != nullptr) {
            return named->name != "Self";
        }

        if (const auto* array_type = type->as<ArrayType>(); array_type != nullptr) {
            return std::holds_alternative<DependentArrayExtent>(array_type->extent) ||
                   has_unresolved_type(array_type->element, visited);
        }

        if (const auto* pointer_type = type->as<PointerType>(); pointer_type != nullptr) {
            return has_unresolved_type(pointer_type->element, visited);
        }

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            return !function_type->generic_parameters.empty();
        }

        if (const auto* nominal = type->as_nominal(); nominal != nullptr) {
            if (!nominal->generic_parameters.empty()) {
                return true;
            }

            for (const auto& argument : nominal->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return true;
                    }
                } else if (has_unresolved_type(argument.type, visited)) {
                    return true;
                }
            }
        }

        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                if (has_unresolved_type(field.type, visited)) {
                    return true;
                }
            }
        }

        return false;
    }

    static bool has_unresolved_type(const Type* type) {
        std::unordered_set<const Type*> visited;
        return has_unresolved_type(type, visited);
    }

  public:
    explicit TypeLowerer(llvm::LLVMContext& context) : context(context) {}

    llvm::Type* lower(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        if (has_unresolved_type(type)) {
            return nullptr;
        }

        if (type->is<IntegerType>()) {
            const auto* integer_type = type->as<IntegerType>();
            return llvm::Type::getIntNTy(context, static_cast<unsigned>(integer_type->size));
        }

        if (type->is<FloatType>()) {
            const auto* float_type = type->as<FloatType>();
            switch (float_type->size) {
            case 16:
                return llvm::Type::getHalfTy(context);
            case 32:
                return llvm::Type::getFloatTy(context);
            case 64:
                return llvm::Type::getDoubleTy(context);
            default:
                return llvm::Type::getFloatTy(context);
            }
        }

        if (type->is<BooleanType>()) {
            return llvm::Type::getInt1Ty(context);
        }

        if (type->is<CharType>()) {
            return llvm::Type::getInt8Ty(context);
        }

        if (type->is<VoidType>() || type->is<NeverType>()) {
            return llvm::Type::getVoidTy(context);
        }

        if (type->is<ArrayType>()) {
            const auto* array_type = type->as<ArrayType>();
            auto* element_type = lower(array_type->element);
            if (element_type == nullptr) {
                return nullptr;
            }

            const auto* concrete = std::get_if<ConcreteArrayExtent>(&array_type->extent);
            if (concrete == nullptr) {
                return llvm::PointerType::getUnqual(context);
            }

            return llvm::ArrayType::get(element_type, concrete->value);
        }

        if (type->is<PointerType>() || type->is<StringType>() || type->is<FunctionType>() ||
            type->is<AnyType>()) {
            return llvm::PointerType::getUnqual(context);
        }

        if (const auto* enum_type = type->as<EnumType>();
            enum_type != nullptr && enum_type->backing_type != nullptr) {
            return lower(enum_type->backing_type);
        }

        if (type->is<StructType>() || type->is<EnumType>() || type->is<InterfaceType>()) {
            auto type_key = type->to_string();
            auto iterator = type_cache.find(type_key);
            if (iterator != type_cache.end()) {
                return iterator->second;
            }

            auto llvm_name = llvm_type_name_for(type);
            auto* llvm_struct = llvm::StructType::create(context, llvm_name);
            type_cache[std::move(type_key)] = llvm_struct;

            populate_type_body(llvm_struct, type);

            return llvm_struct;
        }

        if (type->is<NamedType>()) {
            return nullptr;
        }

        return nullptr;
    }

    llvm::Type* lower_c_abi(const Type* type) {
        if (auto bits = c_abi_integer_struct_bits(type); bits.has_value()) {
            return llvm::Type::getIntNTy(context, *bits);
        }

        return lower(type);
    }

    bool is_c_abi_packed_integer_struct(const Type* type) const {
        return c_abi_integer_struct_bits(type).has_value();
    }

    llvm::FunctionType* lower_function(const FunctionType* function_type,
                                       Abi::Type abi = Abi::Type::Language) {
        if (has_unresolved_type(function_type)) {
            return nullptr;
        }

        const auto parameter_count = function_type->parameters.size();

        std::vector<llvm::Type*> llvm_parameter_types;
        llvm_parameter_types.reserve(parameter_count);

        for (std::size_t index = 0; index < parameter_count; ++index) {
            auto* parameter_type = abi == Abi::Type::C
                                       ? lower_c_abi(function_type->parameters[index].type)
                                       : lower(function_type->parameters[index].type);
            if (parameter_type == nullptr) {
                return nullptr;
            }
            llvm_parameter_types.push_back(parameter_type);
        }

        auto* return_type = abi == Abi::Type::C ? lower_c_abi(function_type->return_type)
                                                : lower(function_type->return_type);
        if (return_type == nullptr) {
            return nullptr;
        }

        return llvm::FunctionType::get(return_type, llvm_parameter_types, function_type->variadic);
    }

    void declare(const std::vector<const TypeSymbol*>& types) {
        for (const auto* type_symbol : types) {
            if (type_symbol == nullptr || type_symbol->type == nullptr ||
                type_symbol->type->as_nominal() == nullptr) {
                continue;
            }

            auto type_key = type_symbol->type->to_string();
            if (!type_cache.contains(type_key)) {
                auto* llvm_struct = llvm::StructType::create(context, type_symbol->name);
                type_cache[std::move(type_key)] = llvm_struct;
            }
        }

        for (const auto* type_symbol : types) {
            if (type_symbol == nullptr || type_symbol->type == nullptr) {
                continue;
            }

            auto type_key = type_symbol->type->to_string();
            if (auto iterator = type_cache.find(type_key); iterator != type_cache.end()) {
                populate_type_body(iterator->second, type_symbol->type);
            }
        }
    }
};
