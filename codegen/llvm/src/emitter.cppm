module;

#include <exception>
#include <expected>
#include <filesystem>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.codegen.llvm.emitter;

import zep.hir.node;
import zep.hir.program;
import zep.frontend.sema.type;
import zep.frontend.node;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.codegen.llvm.type;
import zep.codegen.llvm.scope;
import zep.codegen.llvm.dispatch;
import zep.codegen.llvm.declarations;
import zep.common.logger;

export class FunctionEmitter : public HIRVisitor<llvm::Value*> {
  private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    TypeLowerer& lowerer;
    CodegenScope& scope;
    DispatchBuilder& dispatch;
    DeclarationEmitter& declarations;
    const HIRFunctionDeclaration* current_function = nullptr;

    llvm::Value* equal(llvm::Value* left, llvm::Value* right) {
        if (left == nullptr || right == nullptr) {
            return nullptr;
        }

        if (left->getType()->isFloatingPointTy()) {
            return builder.CreateFCmpOEQ(left, right);
        }

        return builder.CreateICmpEQ(left, right);
    }

    llvm::Value* cast(llvm::Value* source, llvm::Type* target_type, const Type* source_zep_type,
                      const Type* target_zep_type) {
        auto* source_type = source->getType();

        if (source_type->isIntegerTy()) {
            const auto* integer_type =
                source_zep_type != nullptr ? source_zep_type->as<IntegerType>() : nullptr;
            const auto is_signed = integer_type != nullptr ? !integer_type->is_unsigned
                                                           : source_zep_type == nullptr ||
                                                                 !source_zep_type->is<CharType>();

            if (target_type->isIntegerTy()) {
                return builder.CreateIntCast(source, target_type, is_signed);
            }
            if (target_type->isFloatingPointTy()) {
                return is_signed ? builder.CreateSIToFP(source, target_type)
                                 : builder.CreateUIToFP(source, target_type);
            }
            if (target_type->isPointerTy()) {
                return builder.CreateIntToPtr(source, target_type);
            }
        } else if (source_type->isFloatingPointTy()) {
            if (target_type->isFloatingPointTy()) {
                return builder.CreateFPCast(source, target_type);
            }
            const auto* integer_type =
                target_zep_type != nullptr ? target_zep_type->as<IntegerType>() : nullptr;
            const auto is_signed = integer_type == nullptr || !integer_type->is_unsigned;
            if (target_type->isIntegerTy()) {
                return is_signed ? builder.CreateFPToSI(source, target_type)
                                 : builder.CreateFPToUI(source, target_type);
            }
        } else if (source_type->isPointerTy()) {
            if (target_type->isIntegerTy()) {
                return builder.CreatePtrToInt(source, target_type);
            }
        }

        return builder.CreateBitCast(source, target_type);
    }

    llvm::Value* enum_tag(llvm::Value* enum_value) {
        if (enum_value == nullptr) {
            return nullptr;
        }

        return builder.CreateExtractValue(enum_value, {0});
    }

    void return_fallback(const Type* return_type, llvm::Value* last_value) {
        auto* block = builder.GetInsertBlock();
        if (block == nullptr || block->getTerminator() != nullptr) {
            return;
        }

        if (return_type->is<NeverType>()) {
            builder.CreateUnreachable();
        } else if (return_type->is<VoidType>()) {
            builder.CreateRetVoid();
        } else if (last_value != nullptr) {
            builder.CreateRet(last_value);
        } else if (llvm::pred_empty(block)) {
            block->eraseFromParent();
        } else {
            builder.CreateUnreachable();
        }
    }

    void bind_pattern_fields(HIRWhenArm& arm, llvm::Value* subject, const Type* subject_type) {
        const auto* enum_type = subject_type != nullptr ? subject_type->as<EnumType>() : nullptr;
        if (enum_type == nullptr || subject == nullptr) {
            return;
        }

        for (auto& pattern : arm.patterns) {
            if (pattern.pattern_kind != HIRWhenPatternKind::Type::EnumVariant) {
                continue;
            }

            for (const auto& field : pattern.fields) {
                auto index = enum_type->payload_index(pattern.variant_name, field.field_name);
                if (!index.has_value()) {
                    continue;
                }

                auto* value = builder.CreateExtractValue(subject, {static_cast<unsigned>(*index)});
                auto* ptr = builder.CreateAlloca(lowerer.lower(field.type), nullptr);
                builder.CreateStore(value, ptr);
                scope.set(field.binding_name, ptr);
            }

            return;
        }
    }

    llvm::Value* build_when_condition(HIRWhenArm& arm, llvm::Value* subject_value) {
        llvm::Value* result = nullptr;

        for (auto& pattern : arm.patterns) {
            llvm::Value* condition_value = nullptr;

            if (pattern.pattern_kind == HIRWhenPatternKind::Type::Expression) {
                condition_value = visit_expression(*pattern.expression);
                if (condition_value == nullptr) {
                    return nullptr;
                }
                if (subject_value != nullptr) {
                    condition_value = equal(subject_value, condition_value);
                }
            } else {
                if (pattern.variant_value != nullptr) {
                    auto* expected_value = visit_expression(*pattern.variant_value);
                    if (expected_value == nullptr) {
                        return nullptr;
                    }

                    condition_value = equal(subject_value, expected_value);
                } else {
                    auto* tag_value = enum_tag(subject_value);
                    if (tag_value == nullptr) {
                        return nullptr;
                    }
                    auto* expected_tag = llvm::ConstantInt::get(
                        builder.getInt32Ty(), static_cast<uint64_t>(pattern.variant_index));
                    condition_value = builder.CreateICmpEQ(tag_value, expected_tag);
                }
            }

            if (condition_value == nullptr) {
                return nullptr;
            }
            result =
                result == nullptr ? condition_value : builder.CreateOr(result, condition_value);
        }

        if (result == nullptr) {
            return llvm::ConstantInt::getFalse(context);
        }

        return result;
    }

    void emit_when_arm(HIRWhenArm& arm, llvm::Value* subject_value, const Type* subject_type,
                       const Type* result_type, llvm::BasicBlock* merge_block,
                       llvm::BasicBlock* next_block, llvm::Function* function,
                       std::vector<llvm::Value*>& incoming_values,
                       std::vector<llvm::BasicBlock*>& incoming_blocks) {
        auto* body_block = llvm::BasicBlock::Create(context, "", function);

        if (arm.is_else) {
            builder.CreateBr(body_block);
        } else {
            auto* condition_value = build_when_condition(arm, subject_value);
            if (condition_value == nullptr) {
                return;
            }

            if (arm.guard != nullptr) {
                auto* guard_block = llvm::BasicBlock::Create(context, "", function);
                builder.CreateCondBr(condition_value, guard_block, next_block);

                builder.SetInsertPoint(guard_block);
                scope.enter();
                bind_pattern_fields(arm, subject_value, subject_type);
                auto* guard_value = visit_expression(*arm.guard);
                scope.exit();
                if (guard_value == nullptr) {
                    return;
                }

                builder.CreateCondBr(guard_value, body_block, next_block);
            } else {
                builder.CreateCondBr(condition_value, body_block, next_block);
            }
        }

        builder.SetInsertPoint(body_block);
        scope.enter();
        bind_pattern_fields(arm, subject_value, subject_type);
        auto* body_value = visit_statement(*arm.body);
        scope.exit();

        auto reaches_merge = false;
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
            reaches_merge = true;
        }

        auto* incoming_block = builder.GetInsertBlock();
        if (reaches_merge && body_value != nullptr) {
            if (arm.body->type == nullptr || arm.body->type->is<VoidType>()) {
                body_value = nullptr;
            } else if (result_type != nullptr && !result_type->is<VoidType>()) {
                auto* result_llvm_type = lowerer.lower(result_type);
                if (body_value->getType() != result_llvm_type) {
                    body_value = cast(body_value, result_llvm_type, arm.body->type, result_type);
                }
            }

            if (body_value != nullptr) {
                incoming_values.push_back(body_value);
                incoming_blocks.push_back(incoming_block);
            }
        }

        if (next_block != nullptr) {
            builder.SetInsertPoint(next_block);
        }
    }

    void setup_parameters(llvm::Function* function, const FunctionType* function_type) {
        if (function_type == nullptr) {
            return;
        }

        for (std::size_t index = 0; index < function_type->parameters.size(); ++index) {
            const auto& parameter = function_type->parameters[index];
            auto* argument = function->getArg(static_cast<unsigned>(index));
            auto* ptr = allocate(lowerer.lower(parameter.type));
            store(argument, ptr);
            scope.set(parameter.name, ptr);
        }
    }

    llvm::Value* load(llvm::Type* type, llvm::Value* pointer) {
        return builder.CreateLoad(type, pointer);
    }

    llvm::Value* store(llvm::Value* value, llvm::Value* pointer) {
        if (value == nullptr || pointer == nullptr) {
            return nullptr;
        }
        return builder.CreateStore(value, pointer);
    }

    llvm::Value* allocate(llvm::Type* type) { return builder.CreateAlloca(type, nullptr); }

    llvm::Value* pack_c_abi_value(llvm::Value* value, const Type* type) {
        if (value == nullptr || !lowerer.is_c_abi_packed_integer_struct(type)) {
            return value;
        }

        auto* llvm_type = lowerer.lower(type);
        auto* llvm_abi_type = lowerer.lower_c_abi(type);
        if (llvm_type == nullptr || llvm_abi_type == nullptr || llvm_type == llvm_abi_type) {
            return value;
        }

        auto* ptr = allocate(llvm_type);
        store(value, ptr);
        return load(llvm_abi_type, ptr);
    }

    llvm::Value* unpack_c_abi_value(llvm::Value* value, const Type* type) {
        if (value == nullptr || !lowerer.is_c_abi_packed_integer_struct(type)) {
            return value;
        }

        auto* llvm_type = lowerer.lower(type);
        auto* llvm_abi_type = lowerer.lower_c_abi(type);
        if (llvm_type == nullptr || llvm_abi_type == nullptr || llvm_type == llvm_abi_type) {
            return value;
        }

        auto* ptr = allocate(llvm_type);
        store(value, ptr);
        return load(llvm_type, ptr);
    }

    llvm::Value* address(HIRExpression* node) {
        if (node == nullptr) {
            return nullptr;
        }

        if (auto* identifier = node->as<HIRIdentifierExpression>(); identifier != nullptr) {
            return scope.lookup(identifier->name);
        }

        if (auto* member = node->as<HIRMemberExpression>(); member != nullptr) {
            const auto* struct_type = member->object->type->as<StructType>();
            if (struct_type == nullptr) {
                return nullptr;
            }

            auto* object_address = address(member->object);
            if (object_address == nullptr) {
                auto* value = visit_expression(*member->object);
                object_address = allocate(lowerer.lower(struct_type));
                store(value, object_address);
            }

            auto* llvm_struct_type = lowerer.lower(struct_type);
            auto field_index = struct_type->field_index(member->member);
            if (field_index.has_value()) {
                return builder.CreateStructGEP(llvm_struct_type, object_address,
                                               static_cast<unsigned>(*field_index));
            }
            return nullptr;
        }

        if (auto* unary = node->as<HIRUnaryExpression>(); unary != nullptr) {
            if (unary->op == UnaryOperator::Type::Dereference) {
                return visit_expression(*unary->operand);
            }
        }

        if (auto* index = node->as<HIRIndexExpression>(); index != nullptr) {
            auto* index_value = visit_expression(*index->index);
            if (index_value == nullptr) {
                return nullptr;
            }

            const auto* array_type = index->object->type->as<ArrayType>();
            if (array_type != nullptr) {
                auto* object_address = address(index->object);
                if (object_address == nullptr) {
                    return nullptr;
                }

                auto* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
                return builder.CreateGEP(lowerer.lower(array_type), object_address,
                                         {zero, index_value});
            }

            const auto* pointer_type = index->object->type->as<PointerType>();
            if (pointer_type != nullptr) {
                auto* ptr_value = visit_expression(*index->object);
                if (ptr_value == nullptr) {
                    return nullptr;
                }

                auto* element_type = lowerer.lower(pointer_type->element);
                if (element_type == nullptr) {
                    return nullptr;
                }

                return builder.CreateGEP(element_type, ptr_value, {index_value});
            }

            if (index->object->type->is<StringType>()) {
                auto* ptr_value = visit_expression(*index->object);
                if (ptr_value == nullptr) {
                    return nullptr;
                }

                return builder.CreateGEP(builder.getInt8Ty(), ptr_value, {index_value});
            }

            return nullptr;
        }

        return nullptr;
    }

    llvm::Value* visit(HIRBlockStatement& node) override {
        scope.enter();
        llvm::Value* last = nullptr;
        for (auto* statement : node.statements) {
            auto* value = visit_statement(*statement);
            if (value != nullptr) {
                last = value;
            }
        }
        scope.exit();
        return last;
    }

    llvm::Value* visit(HIRBlockExpression& node) override { return visit(*node.body); }

    llvm::Value* visit(HIRStatementGroup& node) override {
        llvm::Value* last = nullptr;
        for (auto* statement : node.statements) {
            auto* value = visit_statement(*statement);
            if (value != nullptr) {
                last = value;
            }
        }
        return last;
    }

    llvm::Value* visit(HIRNumberLiteral& node) override {
        auto* llvm_type = lowerer.lower(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }

        const auto* integer_type = node.type->as<IntegerType>();
        if (integer_type == nullptr) {
            return nullptr;
        }

        const auto base = node.value.starts_with("0x") || node.value.starts_with("0X") ? 16 : 10;
        const auto value = integer_type->is_unsigned
                               ? static_cast<std::uint64_t>(std::stoull(node.value, nullptr, base))
                               : static_cast<std::uint64_t>(std::stoll(node.value, nullptr, base));
        return llvm::ConstantInt::get(llvm_type, value, !integer_type->is_unsigned);
    }

    llvm::Value* visit(HIRFloatLiteral& node) override {
        auto* llvm_type = lowerer.lower(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }
        return llvm::ConstantFP::get(llvm_type, llvm::StringRef(node.value));
    }

    llvm::Value* visit(HIRStringLiteral& node) override {
        return builder.CreateGlobalString(node.value);
    }

    llvm::Value* visit(HIRCharLiteral& node) override {
        return llvm::ConstantInt::get(builder.getInt8Ty(), node.value, false);
    }

    llvm::Value* visit(HIRStringArrayExpression& node) override {
        auto* type = lowerer.lower(node.type);
        auto* array_type = llvm::dyn_cast<llvm::ArrayType>(type);
        if (array_type == nullptr) {
            return nullptr;
        }

        if (node.values.empty()) {
            return llvm::Constant::getNullValue(array_type);
        }

        llvm::Value* value = llvm::UndefValue::get(array_type);
        for (std::size_t index = 0; index < node.values.size(); ++index) {
            auto* item = builder.CreateGlobalString(node.values[index]);
            value = builder.CreateInsertValue(value, item, {static_cast<unsigned>(index)});
        }

        return value;
    }

    llvm::Value* visit(HIRBooleanLiteral& node) override {
        return llvm::ConstantInt::get(context, llvm::APInt(1, node.value ? 1 : 0, false));
    }

    llvm::Value* visit([[maybe_unused]] HIRNullLiteral& node) override {
        return llvm::ConstantPointerNull::get(builder.getPtrTy());
    }

    llvm::Value* visit(HIRIdentifierExpression& node) override {
        auto* ptr = scope.lookup(node.name);
        if (ptr == nullptr) {
            auto* function = declarations.find_function(node.function_symbol, node.name);
            if (function == nullptr && node.function_symbol != nullptr) {
                const auto* function_type =
                    node.type != nullptr ? node.type->as<FunctionType>() : nullptr;
                function = function_type != nullptr
                               ? declarations.declare_function(node.name, function_type,
                                                               llvm::GlobalValue::ExternalLinkage,
                                                               node.function_symbol->abi)
                               : nullptr;
            }
            if (function != nullptr) {
                return function;
            }
            auto* global = module.getGlobalVariable(node.name);
            if (global != nullptr) {
                return load(lowerer.lower(node.type), global);
            }
            return nullptr;
        }

        auto* llvm_type = lowerer.lower(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }
        return load(llvm_type, ptr);
    }

    llvm::Value* visit(HIRBinaryExpression& node) override {
        if (node.left == nullptr || node.right == nullptr) {
            return nullptr;
        }

        auto* left_value = visit_expression(*node.left);
        if (left_value == nullptr) {
            return nullptr;
        }

        if (node.op == BinaryOperator::Type::As) {
            auto* target_type = lowerer.lower(node.type);
            if (target_type == nullptr) {
                return nullptr;
            }

            return cast(left_value, target_type, node.left->type, node.type);
        }

        if (node.op == BinaryOperator::Type::Is) {
            auto* target = node.right->as<HIRTypeExpression>();
            auto* result_type = lowerer.lower(node.type);
            if (target == nullptr || result_type == nullptr || node.left->type == nullptr) {
                return nullptr;
            }

            return llvm::ConstantInt::get(result_type,
                                          Type::same(node.left->type, target->type_value));
        }

        auto* right_value = visit_expression(*node.right);
        if (right_value == nullptr) {
            return nullptr;
        }
        const auto is_float = left_value->getType()->isFloatingPointTy();
        const auto* integer_type = !is_float && node.left != nullptr && node.left->type != nullptr
                                       ? node.left->type->as<IntegerType>()
                                       : nullptr;
        const auto is_unsigned = integer_type != nullptr && integer_type->is_unsigned;

        switch (node.op) {
        case BinaryOperator::Type::Plus:
            return is_float ? builder.CreateFAdd(left_value, right_value)
                            : builder.CreateAdd(left_value, right_value);
        case BinaryOperator::Type::Minus:
            return is_float ? builder.CreateFSub(left_value, right_value)
                            : builder.CreateSub(left_value, right_value);
        case BinaryOperator::Type::Asterisk:
            return is_float ? builder.CreateFMul(left_value, right_value)
                            : builder.CreateMul(left_value, right_value);
        case BinaryOperator::Type::Divide:
            return is_float      ? builder.CreateFDiv(left_value, right_value)
                   : is_unsigned ? builder.CreateUDiv(left_value, right_value)
                                 : builder.CreateSDiv(left_value, right_value);
        case BinaryOperator::Type::Modulo:
            return is_float      ? builder.CreateFRem(left_value, right_value)
                   : is_unsigned ? builder.CreateURem(left_value, right_value)
                                 : builder.CreateSRem(left_value, right_value);
        case BinaryOperator::Type::Equals:
            return is_float ? builder.CreateFCmpOEQ(left_value, right_value)
                            : builder.CreateICmpEQ(left_value, right_value);
        case BinaryOperator::Type::NotEquals:
            return is_float ? builder.CreateFCmpONE(left_value, right_value)
                            : builder.CreateICmpNE(left_value, right_value);
        case BinaryOperator::Type::LessThan:
            return is_float      ? builder.CreateFCmpOLT(left_value, right_value)
                   : is_unsigned ? builder.CreateICmpULT(left_value, right_value)
                                 : builder.CreateICmpSLT(left_value, right_value);
        case BinaryOperator::Type::GreaterThan:
            return is_float      ? builder.CreateFCmpOGT(left_value, right_value)
                   : is_unsigned ? builder.CreateICmpUGT(left_value, right_value)
                                 : builder.CreateICmpSGT(left_value, right_value);
        case BinaryOperator::Type::LessEqual:
            return is_float      ? builder.CreateFCmpOLE(left_value, right_value)
                   : is_unsigned ? builder.CreateICmpULE(left_value, right_value)
                                 : builder.CreateICmpSLE(left_value, right_value);
        case BinaryOperator::Type::GreaterEqual:
            return is_float      ? builder.CreateFCmpOGE(left_value, right_value)
                   : is_unsigned ? builder.CreateICmpUGE(left_value, right_value)
                                 : builder.CreateICmpSGE(left_value, right_value);
        case BinaryOperator::Type::And:
            return builder.CreateAnd(left_value, right_value);
        case BinaryOperator::Type::Or:
            return builder.CreateOr(left_value, right_value);
        default:
            return nullptr;
        }
    }

    llvm::Value* visit(HIRUnaryExpression& node) override {
        auto* operand_value = visit_expression(*node.operand);
        if (operand_value == nullptr) {
            return nullptr;
        }

        switch (node.op) {
        case UnaryOperator::Type::Minus:
            return operand_value->getType()->isFloatingPointTy() ? builder.CreateFNeg(operand_value)
                                                                 : builder.CreateNeg(operand_value);
        case UnaryOperator::Type::Not:
            return builder.CreateNot(operand_value);
        case UnaryOperator::Type::Dereference:
            return load(lowerer.lower(node.type), operand_value);
        case UnaryOperator::Type::AddressOf:
        case UnaryOperator::Type::AddressOfMut:
            return address(node.operand);
        default:
            return nullptr;
        }
    }

    llvm::Value* visit(HIRCoerceExpression& node) override {
        if (node.coercion == Coercion::Type::None) {
            return visit_expression(*node.value);
        }

        if (node.coercion == Coercion::Type::BaseSlice) {
            const auto* target_struct = node.type->as<StructType>();
            const auto* source_struct =
                node.source_type != nullptr ? node.source_type->as<StructType>() : nullptr;
            if (target_struct == nullptr || source_struct == nullptr) {
                return nullptr;
            }

            auto* source_value = visit_expression(*node.value);
            if (source_value == nullptr) {
                return nullptr;
            }

            auto* target_type = lowerer.lower(target_struct);
            auto* target = allocate(target_type);

            for (const auto& field : target_struct->fields) {
                auto target_index = target_struct->field_index(field.name);
                auto source_index = source_struct->field_index(field.name);
                if (!target_index.has_value() || !source_index.has_value()) {
                    continue;
                }

                auto* value = builder.CreateExtractValue(source_value,
                                                         {static_cast<unsigned>(*source_index)});
                auto* field_ptr = builder.CreateStructGEP(target_type, target,
                                                          static_cast<unsigned>(*target_index));
                store(value, field_ptr);
            }

            return load(target_type, target);
        }

        if (node.coercion == Coercion::Type::InterfaceValue) {
            const auto* interface_type = node.type->as<InterfaceType>();
            const auto* source_pointer =
                node.source_type != nullptr ? node.source_type->as<PointerType>() : nullptr;
            const auto* source_struct =
                node.source_type != nullptr ? node.source_type->as<StructType>() : nullptr;
            if (source_struct == nullptr && source_pointer != nullptr &&
                source_pointer->element != nullptr) {
                source_struct = source_pointer->element->as<StructType>();
            }
            if (interface_type == nullptr || source_struct == nullptr) {
                return nullptr;
            }

            auto* object_address =
                source_pointer != nullptr ? visit_expression(*node.value) : address(node.value);
            if (object_address == nullptr && source_pointer == nullptr) {
                auto* source_value = visit_expression(*node.value);
                if (source_value == nullptr) {
                    return nullptr;
                }

                object_address = allocate(lowerer.lower(source_struct));
                store(source_value, object_address);
            }

            auto* interface_llvm_type = lowerer.lower(interface_type);
            auto* value = allocate(interface_llvm_type);

            auto* object_ptr = builder.CreateStructGEP(interface_llvm_type, value, 0);
            store(object_address, object_ptr);

            auto* method_table = dispatch.get(source_struct, interface_type);
            if (method_table != nullptr) {
                auto* method_table_ptr = builder.CreateStructGEP(interface_llvm_type, value, 1);
                store(method_table, method_table_ptr);
            }

            return load(interface_llvm_type, value);
        }

        return visit_expression(*node.value);
    }

    llvm::Value* visit(HIRDropFlagClearExpression& node) override {
        auto* value = visit_expression(*node.value);
        if (value == nullptr) {
            return nullptr;
        }

        auto* flag = scope.lookup(node.drop_flag_name);
        if (flag != nullptr) {
            auto* cleared = llvm::ConstantInt::get(context, llvm::APInt(1, 0, false));
            store(cleared, flag);
        }

        return value;
    }

    llvm::Value* load_method_table_entry(llvm::Value* method_table, std::size_t slot) {
        if (method_table == nullptr) {
            return nullptr;
        }

        auto* index = llvm::ConstantInt::get(builder.getInt64Ty(), slot);
        auto* entry_ptr = builder.CreateGEP(builder.getPtrTy(), method_table, index);
        return load(builder.getPtrTy(), entry_ptr);
    }

    bool dynamic_interface_parts(HIRCallExpression& node, llvm::Value*& object,
                                 llvm::Value*& method_table) {
        auto* receiver = !node.arguments.empty() ? node.arguments.front() : nullptr;
        if (auto* unary = receiver != nullptr ? receiver->as<HIRUnaryExpression>() : nullptr;
            unary != nullptr && (unary->op == UnaryOperator::Type::AddressOf ||
                                 unary->op == UnaryOperator::Type::AddressOfMut)) {
            receiver = unary->operand;
        }

        if (receiver == nullptr) {
            return false;
        }

        auto* value = visit_expression(*receiver);
        if (value == nullptr) {
            return false;
        }

        object = builder.CreateExtractValue(value, {0});
        method_table = builder.CreateExtractValue(value, {1});
        return object != nullptr && method_table != nullptr;
    }

    llvm::Value* visit_dynamic_call(HIRCallExpression& node, const FunctionType* function_type,
                                    std::size_t slot) {
        llvm::Value* object = nullptr;
        llvm::Value* method_table = nullptr;

        if (!dynamic_interface_parts(node, object, method_table)) {
            return nullptr;
        }

        auto* callee = load_method_table_entry(method_table, slot);
        if (callee == nullptr) {
            return nullptr;
        }

        std::vector<llvm::Value*> llvm_arguments;
        llvm_arguments.reserve(node.arguments.size());
        llvm_arguments.push_back(object);

        for (std::size_t index = 1; index < node.arguments.size(); ++index) {
            auto* argument_value = visit_expression(*node.arguments[index]);
            if (argument_value == nullptr) {
                return nullptr;
            }

            llvm_arguments.push_back(argument_value);
        }

        auto* llvm_function_type = lowerer.lower_function(function_type);
        auto* instruction = builder.CreateCall(llvm_function_type, callee, llvm_arguments);

        if (function_type->return_type->is<NeverType>()) {
            instruction->addFnAttr(llvm::Attribute::NoReturn);
            builder.CreateUnreachable();
        }

        return instruction;
    }

    llvm::Value* visit(HIRCallExpression& node) override {
        if (node.target == nullptr) {
            return nullptr;
        }

        if (const auto* intrinsic = node.target->as<HIRIntrinsicCallTarget>();
            intrinsic != nullptr) {
            if (intrinsic->intrinsic != Intrinsic::Type::InlineAssembly ||
                node.arguments.size() != 1) {
                return nullptr;
            }
            auto* argument = node.arguments.front()->as<HIRStringLiteral>();
            if (argument == nullptr) {
                return nullptr;
            }
            auto* asm_fn =
                llvm::InlineAsm::get(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
                                     argument->value, "", true);
            return builder.CreateCall(asm_fn);
        }

        const FunctionType* function_type = nullptr;
        llvm::Value* llvm_callee = nullptr;
        Abi::Type abi = Abi::Type::Language;
        if (const auto* direct = node.target->as<HIRDirectCallTarget>(); direct != nullptr) {
            if (direct->function_symbol == nullptr) {
                return nullptr;
            }
            function_type = direct->function_type;
            abi = direct->function_symbol->abi;
            llvm_callee = declarations.find_function(direct->function_symbol, direct->emitted_name);
            if (llvm_callee == nullptr && !direct->emitted_name.empty()) {
                llvm_callee =
                    declarations.declare_function(direct->emitted_name, direct->function_type,
                                                  llvm::GlobalValue::ExternalLinkage, abi);
            }
        } else if (const auto* interface_target = node.target->as<HIRInterfaceCallTarget>();
                   interface_target != nullptr) {
            return visit_dynamic_call(node, interface_target->method_symbol->function_type,
                                      interface_target->slot);
        } else if (const auto* indirect = node.target->as<HIRIndirectCallTarget>();
                   indirect != nullptr) {
            function_type = indirect->function_type;
            llvm_callee = visit_expression(*indirect->callee);
        }
        if (function_type == nullptr || llvm_callee == nullptr) {
            return nullptr;
        }

        auto* llvm_function_type = lowerer.lower_function(function_type, abi);
        if (llvm_function_type == nullptr) {
            return nullptr;
        }

        if (auto* function = llvm::dyn_cast<llvm::Function>(llvm_callee);
            function != nullptr && function->getFunctionType() != llvm_function_type) {
            return nullptr;
        }

        std::vector<llvm::Value*> llvm_arguments;
        llvm_arguments.reserve(node.arguments.size());

        for (std::size_t index = 0; index < node.arguments.size(); ++index) {
            auto* argument = node.arguments[index];
            auto* argument_value = visit_expression(*argument);
            if (argument_value == nullptr) {
                return nullptr;
            }

            if (abi == Abi::Type::C && index < function_type->parameters.size()) {
                argument_value =
                    pack_c_abi_value(argument_value, function_type->parameters[index].type);
            }

            llvm_arguments.push_back(argument_value);
        }

        auto* instruction = builder.CreateCall(llvm_function_type, llvm_callee, llvm_arguments);

        if (function_type->return_type->is<NeverType>()) {
            instruction->addFnAttr(llvm::Attribute::NoReturn);
            builder.CreateUnreachable();
            return instruction;
        }

        if (abi == Abi::Type::C) {
            return unpack_c_abi_value(instruction, function_type->return_type);
        }

        return instruction;
    }

    llvm::Value* visit(HIRIndexExpression& node) override {
        auto* ptr = address(&node);
        if (ptr == nullptr) {
            return nullptr;
        }
        return load(lowerer.lower(node.type), ptr);
    }

    llvm::Value* visit(HIRMemberExpression& node) override {
        if (node.member == "value" && node.object != nullptr && node.object->type != nullptr &&
            node.object->type->is<EnumType>()) {
            auto* value = visit_expression(*node.object);
            return enum_tag(value);
        }

        auto* value = address(&node);
        if (value == nullptr) {
            return nullptr;
        }
        return load(lowerer.lower(node.type), value);
    }

    llvm::Value* visit(HIRAssignExpression& node) override {
        auto* value = visit_expression(*node.value);
        auto* target_value = address(node.target);
        if (value == nullptr || target_value == nullptr) {
            return nullptr;
        }
        return store(value, target_value);
    }

    llvm::Value* visit(HIRStructLiteralExpression& node) override {
        const auto* struct_type = node.type->as<StructType>();
        auto* llvm_type = lowerer.lower(struct_type);
        auto* value = allocate(llvm_type);

        for (const auto& field : node.fields) {
            auto* field_value = visit_expression(*field.value);
            if (field_value == nullptr) {
                return nullptr;
            }

            auto field_index = struct_type->field_index(field.name);
            if (field_index.has_value()) {
                auto* field_ptr =
                    builder.CreateStructGEP(llvm_type, value, static_cast<unsigned>(*field_index));
                store(field_value, field_ptr);
            }
        }

        return load(llvm_type, value);
    }

    llvm::Value* visit(HIREnumVariantExpression& node) override {
        const auto* enum_type = node.type->as<EnumType>();
        if (enum_type == nullptr) {
            return nullptr;
        }

        auto* llvm_type = lowerer.lower(enum_type);
        auto* value = allocate(llvm_type);

        store(llvm::Constant::getNullValue(llvm_type), value);

        auto* tag_ptr = builder.CreateStructGEP(llvm_type, value, 0);
        auto* tag_value = llvm::ConstantInt::get(builder.getInt32Ty(), node.variant_index);
        store(tag_value, tag_ptr);

        for (const auto& field : node.fields) {
            auto* field_value = visit_expression(*field.value);
            if (field_value == nullptr) {
                return nullptr;
            }

            auto index = enum_type->payload_index(node.variant_name, field.name);
            if (!index.has_value()) {
                continue;
            }

            auto* field_ptr =
                builder.CreateStructGEP(llvm_type, value, static_cast<unsigned>(*index));
            store(field_value, field_ptr);
        }

        return load(llvm_type, value);
    }

    llvm::Value* visit(HIRArrayLiteralExpression& node) override {
        const auto* array_type = node.type->as<ArrayType>();
        if (array_type == nullptr) {
            return nullptr;
        }

        auto* llvm_type = lowerer.lower(array_type);
        auto* value = allocate(llvm_type);

        for (std::size_t i = 0; i < node.elements.size(); ++i) {
            auto* element_value = visit_expression(*node.elements[i]);
            if (element_value == nullptr) {
                return nullptr;
            }

            auto* element_ptr = builder.CreateStructGEP(llvm_type, value, static_cast<unsigned>(i));
            store(element_value, element_ptr);
        }

        return load(llvm_type, value);
    }

    llvm::Value* visit(HIRIfExpression& node) override {
        if (node.condition == nullptr || node.condition->type == nullptr) {
            return nullptr;
        }

        auto* condition_value = visit_expression(*node.condition);
        if (condition_value == nullptr) {
            return nullptr;
        }

        auto* function = builder.GetInsertBlock()->getParent();

        auto* then_block = llvm::BasicBlock::Create(context, "", function);
        auto* else_block = llvm::BasicBlock::Create(context, "");
        auto* merge_block = llvm::BasicBlock::Create(context, "");

        builder.CreateCondBr(condition_value, then_block, else_block);

        builder.SetInsertPoint(then_block);
        auto* then_value = visit_statement(*node.then_branch);
        auto then_reaches_merge = builder.GetInsertBlock()->getTerminator() == nullptr;
        if (then_reaches_merge) {
            builder.CreateBr(merge_block);
        }
        then_block = builder.GetInsertBlock();

        function->insert(function->end(), else_block);
        builder.SetInsertPoint(else_block);

        llvm::Value* else_value = nullptr;
        if (node.else_branch != nullptr) {
            else_value = visit_statement(*node.else_branch);
        }
        auto else_reaches_merge = builder.GetInsertBlock()->getTerminator() == nullptr;
        if (else_reaches_merge) {
            builder.CreateBr(merge_block);
        }
        else_block = builder.GetInsertBlock();

        function->insert(function->end(), merge_block);
        builder.SetInsertPoint(merge_block);

        if (node.type != nullptr && !node.type->is<VoidType>()) {
            auto* llvm_type = lowerer.lower(node.type);
            auto* phi = builder.CreatePHI(
                llvm_type, static_cast<unsigned>(then_reaches_merge + else_reaches_merge));

            if (then_reaches_merge) {
                if (node.then_branch->type == nullptr || node.then_branch->type->is<VoidType>()) {
                    then_value = nullptr;
                } else if (then_value != nullptr && then_value->getType() != llvm_type) {
                    then_value = cast(then_value, llvm_type, node.then_branch->type, node.type);
                }
                phi->addIncoming(then_value != nullptr ? then_value
                                                       : llvm::UndefValue::get(llvm_type),
                                 then_block);
            }

            if (else_reaches_merge) {
                if (node.else_branch == nullptr || node.else_branch->type == nullptr ||
                    node.else_branch->type->is<VoidType>()) {
                    else_value = nullptr;
                } else if (else_value != nullptr && else_value->getType() != llvm_type) {
                    else_value = cast(else_value, llvm_type, node.else_branch->type, node.type);
                }
                phi->addIncoming(else_value != nullptr ? else_value
                                                       : llvm::UndefValue::get(llvm_type),
                                 else_block);
            }

            return phi;
        }

        return nullptr;
    }

    llvm::Value* visit(HIRWhenExpression& node) override {
        auto* subject_value = node.subject != nullptr ? visit_expression(*node.subject) : nullptr;
        if (node.subject != nullptr && subject_value == nullptr) {
            return nullptr;
        }

        auto* function = builder.GetInsertBlock()->getParent();
        auto* merge_block = llvm::BasicBlock::Create(context, "");

        std::vector<llvm::Value*> incoming_values;
        std::vector<llvm::BasicBlock*> incoming_blocks;
        incoming_values.reserve(node.arms.size());
        incoming_blocks.reserve(node.arms.size());

        for (std::size_t i = 0; i < node.arms.size(); ++i) {
            auto& arm = node.arms[i];
            auto* next_block =
                arm.is_else ? nullptr : llvm::BasicBlock::Create(context, "", function);

            emit_when_arm(arm, subject_value,
                          node.subject != nullptr ? node.subject->type : nullptr, node.type,
                          merge_block, next_block, function, incoming_values, incoming_blocks);
        }

        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            if (node.is_exhaustive) {
                builder.CreateUnreachable();
            } else {
                builder.CreateBr(merge_block);
            }
        }

        function->insert(function->end(), merge_block);
        builder.SetInsertPoint(merge_block);

        if (node.type != nullptr && !node.type->is<VoidType>()) {
            auto incoming_count = static_cast<unsigned>(incoming_values.size());
            auto* phi = builder.CreatePHI(lowerer.lower(node.type), incoming_count);

            for (std::size_t i = 0; i < incoming_values.size(); ++i) {
                phi->addIncoming(incoming_values[i], incoming_blocks[i]);
            }

            return phi;
        }

        return nullptr;
    }

    llvm::Value* visit(HIRTypeExpression& node [[maybe_unused]]) override { return nullptr; }

    llvm::Value* visit(HIRExpressionStatement& node) override {
        if (node.expression == nullptr || node.expression->type == nullptr) {
            return nullptr;
        }

        auto* value = visit_expression(*node.expression);
        if (value == nullptr && !node.expression->type->is<VoidType>()) {
            return nullptr;
        }

        return value;
    }

    llvm::Value* visit(HIRReturnStatement& node) override {
        if (node.value == nullptr) {
            return builder.CreateRetVoid();
        }
        auto* value = visit_expression(*node.value);
        if (value == nullptr) {
            return nullptr;
        }
        return builder.CreateRet(value);
    }

    llvm::Value* visit(HIRLoopStatement& node) override {
        auto* function = builder.GetInsertBlock()->getParent();

        scope.enter();

        for (auto* initializer : node.initializers) {
            visit_statement(*initializer);
        }

        auto* condition_block = llvm::BasicBlock::Create(context, "", function);
        auto* body_block = llvm::BasicBlock::Create(context, "", function);
        auto* step_block = llvm::BasicBlock::Create(context, "", function);
        auto* merge_block = llvm::BasicBlock::Create(context, "", function);

        builder.CreateBr(condition_block);

        builder.SetInsertPoint(condition_block);
        if (node.condition == nullptr || node.condition->type == nullptr) {
            return nullptr;
        }

        auto* condition_value = visit_expression(*node.condition);
        if (condition_value == nullptr) {
            return nullptr;
        }
        builder.CreateCondBr(condition_value, body_block, merge_block);

        builder.SetInsertPoint(body_block);
        visit_statement(*node.body);
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(step_block);
        }

        builder.SetInsertPoint(step_block);
        if (node.step != nullptr) {
            visit_expression(*node.step);
        }
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(condition_block);
        }

        builder.SetInsertPoint(merge_block);

        scope.exit();

        auto* boolean_condition =
            node.condition != nullptr ? node.condition->as<HIRBooleanLiteral>() : nullptr;
        if (boolean_condition != nullptr && boolean_condition->value) {
            builder.CreateUnreachable();
        }

        return nullptr;
    }

    llvm::Value* visit(HIRVarDeclaration& node) override {
        if (node.is_global) {
            return scope.lookup(node.name);
        }

        auto* type = lowerer.lower(node.type);
        if (type == nullptr) {
            return nullptr;
        }

        auto* ptr = allocate(type);
        scope.set(node.name, ptr);

        if (node.initializer != nullptr) {
            auto* value = visit_expression(*node.initializer);
            if (value == nullptr) {
                return nullptr;
            }
            store(value, ptr);
        }

        return ptr;
    }

    llvm::Value* visit(HIRFunctionDeclaration& node) override {
        auto* function = declarations.find_function(node.function_symbol, node.name);
        if (function == nullptr) {
            return nullptr;
        }
        if (node.body == nullptr) {
            return function;
        }

        auto* entry = llvm::BasicBlock::Create(context, "", function);
        builder.SetInsertPoint(entry);

        scope.enter();
        setup_parameters(function, node.type->as<FunctionType>());
        auto* saved_current_function = current_function;
        current_function = &node;
        auto* last_value = visit_statement(*node.body);
        current_function = saved_current_function;
        scope.exit();

        return_fallback(node.return_type, last_value);

        llvm::verifyFunction(*function);

        return function;
    }

  public:
    explicit FunctionEmitter(llvm::LLVMContext& context, llvm::Module& module,
                             llvm::IRBuilder<>& builder, TypeLowerer& lowerer, CodegenScope& scope,
                             DispatchBuilder& dispatch, DeclarationEmitter& declarations)
        : context(context), module(module), builder(builder), lowerer(lowerer), scope(scope),
          dispatch(dispatch), declarations(declarations) {}

    llvm::Value* emit_function(HIRFunctionDeclaration& node) { return visit(node); }

    void emit_definitions(const HIRProgram& program) {
        for (auto* statement : program.statements) {
            if (statement->as<HIRVarDeclaration>() != nullptr) {
                continue;
            }

            visit_statement(*statement);
        }
    }
};
