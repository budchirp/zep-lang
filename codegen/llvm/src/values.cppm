module;

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <cstdint>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.llvm.values;

import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.hir.node;
import zep.codegen.llvm.types;

export class ValueScope {
  private:
    std::vector<std::unordered_map<std::string, llvm::Value*>> values;

  public:
    ValueScope() {
        values.emplace_back();
    }

    void enter() {
        values.emplace_back();
    }

    void exit() {
        values.pop_back();
    }

    void set(const std::string& name, llvm::Value* value) {
        values.back()[name] = value;
    }

    llvm::Value* lookup(const std::string& name) {
        for (auto& frame : std::views::reverse(values)) {
            if (const auto found = frame.find(name); found != frame.end()) {
                return found->second;
            }
        }

        return nullptr;
    }
};

export class Places {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    ValueScope& scope;
    HIRVisitor<llvm::Value*>& expressions;

  public:
    Places(llvm::IRBuilder<>& builder, Types& types, ValueScope& scope,
           HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), scope(scope), expressions(expressions) {}

    llvm::Value* allocate(llvm::Type* type) {
        auto& entry = builder.GetInsertBlock()->getParent()->getEntryBlock();
        llvm::IRBuilder<> allocations(&entry, entry.begin());

        return allocations.CreateAlloca(type, nullptr);
    }

    llvm::Value* address(HIRExpression* expression) {
        if (expression == nullptr) {
            return nullptr;
        }

        if (const auto* identifier = expression->as<HIRIdentifierExpression>(); identifier != nullptr) {
            return scope.lookup(identifier->name);
        }

        if (auto* member = expression->as<HIRMemberExpression>(); member != nullptr) {
            const auto* structure = member->object->type->as<StructType>();

            if (structure == nullptr) {
                return nullptr;
            }

            auto* storage = address(member->object);

            if (storage == nullptr) {
                auto* value = expressions.visit_expression(*member->object);
                storage = allocate(types.lower(structure));

                if (value == nullptr) {
                    return nullptr;
                }

                builder.CreateStore(value, storage);
            }

            const auto field = structure->field_index(member->member);

            if (!field.has_value()) {
                return nullptr;
            }

            return builder.CreateStructGEP(types.lower(structure), storage,
                                           static_cast<unsigned>(*field));
        }

        if (auto* unary = expression->as<HIRUnaryExpression>(); unary != nullptr &&
            unary->op == UnaryOperator::Type::Dereference) {
            return expressions.visit_expression(*unary->operand);
        }

        auto* index = expression->as<HIRIndexExpression>();

        if (index == nullptr) {
            return nullptr;
        }

        auto* offset = expressions.visit_expression(*index->index);

        if (offset == nullptr) {
            return nullptr;
        }

        const auto* type = index->object->type;

        if (const auto* array = type->as<ArrayType>(); array != nullptr) {
            auto* storage = address(index->object);

            if (storage == nullptr) {
                return nullptr;
            }

            auto* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);

            return builder.CreateGEP(types.lower(array), storage, {zero, offset});
        }

        llvm::Type* element = nullptr;

        if (const auto* pointer = type->as<PointerType>(); pointer != nullptr) {
            element = types.lower(pointer->element);
        } else if (type->is<StringType>()) {
            element = builder.getInt8Ty();
        }

        if (element == nullptr) {
            return nullptr;
        }

        auto* storage = expressions.visit_expression(*index->object);

        if (storage == nullptr) {
            return nullptr;
        }

        return builder.CreateGEP(element, storage, {offset});
    }
};

export class Conversions {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    Places& places;

  public:
    class Form {
      public:
        enum class Type : std::uint8_t { Native, External };
    };

    Conversions(llvm::IRBuilder<>& builder, Types& types, Places& places)
        : builder(builder), types(types), places(places) {}

    llvm::Value* cast(llvm::Value* value, llvm::Type* target,
                      const Type* source_type, const Type* target_type) {
        auto* source = value->getType();

        if (source->isIntegerTy()) {
            const auto* integer = source_type != nullptr ? source_type->as<IntegerType>() : nullptr;
            const auto is_signed = integer != nullptr ? !integer->is_unsigned
                : source_type == nullptr || !source_type->is<CharType>();

            if (target->isIntegerTy()) {
                return builder.CreateIntCast(value, target, is_signed);
            }

            if (target->isFloatingPointTy()) {
                return is_signed ? builder.CreateSIToFP(value, target)
                                 : builder.CreateUIToFP(value, target);
            }

            if (target->isPointerTy()) {
                return builder.CreateIntToPtr(value, target);
            }
        } else if (source->isFloatingPointTy()) {
            if (target->isFloatingPointTy()) {
                return builder.CreateFPCast(value, target);
            }

            const auto* integer = target_type != nullptr ? target_type->as<IntegerType>() : nullptr;
            const auto is_signed = integer == nullptr || !integer->is_unsigned;

            if (target->isIntegerTy()) {
                return is_signed ? builder.CreateFPToSI(value, target)
                                 : builder.CreateFPToUI(value, target);
            }
        } else if (source->isPointerTy() && target->isIntegerTy()) {
            return builder.CreatePtrToInt(value, target);
        }

        return builder.CreateBitCast(value, target);
    }

    llvm::Value* abi(llvm::Value* value, const Type* type, Form::Type form) {
        if (value == nullptr || !types.is_c_abi_packed_integer_struct(type)) {
            return value;
        }

        auto* native = types.lower(type);
        auto* external = types.lower_c_abi(type);

        if (native == nullptr || external == nullptr || native == external) {
            return value;
        }

        auto* storage = places.allocate(native);
        builder.CreateStore(value, storage);

        return builder.CreateLoad(form == Form::Type::External ? external : native, storage);
    }
};

export class Aggregates {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    Places& places;
    HIRVisitor<llvm::Value*>& expressions;

  public:
    Aggregates(llvm::IRBuilder<>& builder, Types& types, Places& places,
               HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), places(places), expressions(expressions) {}

    llvm::Value* emit(HIRStructLiteralExpression& expression) {
        const auto* structure = expression.type->as<StructType>();
        auto* type = types.lower(structure);
        auto* storage = places.allocate(type);

        for (const auto& field : expression.fields) {
            auto* value = expressions.visit_expression(*field.value);

            if (value == nullptr) {
                return nullptr;
            }

            const auto index = structure->field_index(field.name);

            if (index.has_value()) {
                auto* destination = builder.CreateStructGEP(
                    type, storage, static_cast<unsigned>(*index));

                builder.CreateStore(value, destination);
            }
        }

        return builder.CreateLoad(type, storage);
    }

    llvm::Value* emit(HIREnumVariantExpression& expression) {
        const auto* enumeration = expression.type->as<EnumType>();

        if (enumeration == nullptr) {
            return nullptr;
        }

        auto* type = types.lower(enumeration);
        auto* storage = places.allocate(type);
        builder.CreateStore(llvm::Constant::getNullValue(type), storage);

        auto* tag = builder.CreateStructGEP(type, storage, 0);
        auto* value = llvm::ConstantInt::get(builder.getInt32Ty(), expression.variant_index);
        builder.CreateStore(value, tag);

        for (const auto& field : expression.fields) {
            auto* value = expressions.visit_expression(*field.value);

            if (value == nullptr) {
                return nullptr;
            }

            const auto index = enumeration->payload_index(expression.variant_name, field.name);

            if (index.has_value()) {
                auto* destination = builder.CreateStructGEP(
                    type, storage, static_cast<unsigned>(*index));

                builder.CreateStore(value, destination);
            }
        }

        return builder.CreateLoad(type, storage);
    }

    llvm::Value* emit(HIRArrayLiteralExpression& expression) {
        const auto* array = expression.type->as<ArrayType>();

        if (array == nullptr) {
            return nullptr;
        }

        auto* type = types.lower(array);
        auto* storage = places.allocate(type);
        auto* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);

        for (auto index = std::size_t{0}; index < expression.elements.size(); ++index) {
            auto* value = expressions.visit_expression(*expression.elements[index]);

            if (value == nullptr) {
                return nullptr;
            }

            auto* offset = llvm::ConstantInt::get(builder.getInt64Ty(), index);
            auto* destination = builder.CreateGEP(type, storage, {zero, offset});
            builder.CreateStore(value, destination);
        }

        return builder.CreateLoad(type, storage);
    }
};
