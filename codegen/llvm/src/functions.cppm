module;

#include <cstddef>
#include <cstdint>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <string>
#include <vector>

export module zep.codegen.llvm.functions;

import zep.codegen.llvm.assembly;
import zep.codegen.llvm.calls;
import zep.codegen.llvm.values;
import zep.codegen.llvm.control_flow;

import zep.hir.node;
import zep.hir.program;
import zep.frontend.sema.types;
import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.symbols;
import zep.codegen.llvm.types;
import zep.codegen.llvm.declarations;

export class Functions : public HIRVisitor<llvm::Value*> {
  private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    Types& lowerer;
    ValueScope& scope;
    Dispatch& dispatch;
    Declarations& declarations;
    Places places;
    Conversions conversions;
    Calls calls;
    Assembly assembly;
    Aggregates aggregates;
    ControlFlow flow;

    llvm::Value* equal(llvm::Value* left, llvm::Value* right) {
        if (left == nullptr || right == nullptr) {
            return nullptr;
        }

        if (left->getType()->isFloatingPointTy()) {
            return builder.CreateFCmpOEQ(left, right);
        }

        return builder.CreateICmpEQ(left, right);
    }

    llvm::Value* enum_tag(llvm::Value* enum_value) {
        if (enum_value == nullptr) {
            return nullptr;
        }

        return builder.CreateExtractValue(enum_value, {0});
    }

    void return_fallback(const Type* return_type, llvm::Value* last_value) {
        auto* block = builder.GetInsertBlock();
        if (block == nullptr || block->hasTerminator()) {
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

    void bind_pattern_fields(HIRWhenArm& arm, llvm::Value* subject, const Type* subject_type, llvm::Value* subject_address) {
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

                if (subject_address != nullptr) {
                    scope.set(field.binding_name, builder.CreateStructGEP(lowerer.lower(enum_type), subject_address,
                        static_cast<unsigned>(*index)));
                    continue;
                }

                auto* value = builder.CreateExtractValue(subject, {static_cast<unsigned>(*index)});
                auto* ptr = places.allocate(lowerer.lower(field.type));
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
                       llvm::Value* subject_address, const Type* result_type, llvm::BasicBlock* merge_block,
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
                bind_pattern_fields(arm, subject_value, subject_type, subject_address);
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
        bind_pattern_fields(arm, subject_value, subject_type, subject_address);
        auto* body_value = visit_statement(*arm.body);
        scope.exit();

        auto reaches_merge = false;
        if (!builder.GetInsertBlock()->hasTerminator()) {
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
                    body_value = conversions.cast(body_value, result_llvm_type, arm.body->type, result_type);
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
            auto* ptr = places.allocate(lowerer.lower(parameter.type));
            builder.CreateStore(argument, ptr);
            scope.set(parameter.name, ptr);
        }
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
            auto* function = declarations.find_function(node.name);
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
                return builder.CreateLoad(lowerer.lower(node.type), global);
            }
            return nullptr;
        }

        auto* llvm_type = lowerer.lower(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }
        return builder.CreateLoad(llvm_type, ptr);
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

            return conversions.cast(left_value, target_type, node.left->type, node.type);
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

        if (node.op == BinaryOperator::Type::And || node.op == BinaryOperator::Type::Or) {
            auto* function = builder.GetInsertBlock()->getParent();
            auto* left_block = builder.GetInsertBlock();
            auto* right_block = llvm::BasicBlock::Create(context, "", function);
            auto* merge_block = llvm::BasicBlock::Create(context, "", function);
            const auto conjunction = node.op == BinaryOperator::Type::And;
            builder.CreateCondBr(left_value, conjunction ? right_block : merge_block,
                                 conjunction ? merge_block : right_block);

            builder.SetInsertPoint(right_block);
            auto* right_value = visit_expression(*node.right);
            auto* right_end = builder.GetInsertBlock();
            const auto reaches_merge = !right_end->hasTerminator();
            if (reaches_merge) {
                builder.CreateBr(merge_block);
            }

            builder.SetInsertPoint(merge_block);
            auto* result = builder.CreatePHI(builder.getInt1Ty(), reaches_merge ? 2U : 1U);
            result->addIncoming(builder.getInt1(!conjunction), left_block);
            if (reaches_merge) {
                result->addIncoming(right_value != nullptr ? right_value : builder.getFalse(), right_end);
            }
            return result;
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
        if (node.op == UnaryOperator::Type::AddressOf || node.op == UnaryOperator::Type::AddressOfMut) {
            auto* pointer = places.address(node.operand);
            if (pointer != nullptr) {
                return pointer;
            }
            auto* value = visit_expression(*node.operand);
            if (value == nullptr) {
                return nullptr;
            }
            auto* temporary = places.allocate(lowerer.lower(node.operand->type));
            builder.CreateStore(value, temporary);
            return temporary;
        }
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
            return builder.CreateLoad(lowerer.lower(node.type), operand_value);
        case UnaryOperator::Type::AddressOf:
        case UnaryOperator::Type::AddressOfMut:
            return places.address(node.operand);
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
            auto* target = places.allocate(target_type);

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
                builder.CreateStore(value, field_ptr);
            }

            return builder.CreateLoad(target_type, target);
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
                source_pointer != nullptr ? visit_expression(*node.value) : places.address(node.value);
            if (object_address == nullptr && source_pointer == nullptr) {
                auto* source_value = visit_expression(*node.value);
                if (source_value == nullptr) {
                    return nullptr;
                }

                object_address = places.allocate(lowerer.lower(source_struct));
                builder.CreateStore(source_value, object_address);
            }

            auto* interface_llvm_type = lowerer.lower(interface_type);
            auto* value = places.allocate(interface_llvm_type);

            auto* object_ptr = builder.CreateStructGEP(interface_llvm_type, value, 0);
            builder.CreateStore(object_address, object_ptr);

            auto* method_table = dispatch.get(source_struct, interface_type);
            if (method_table != nullptr) {
                auto* method_table_ptr = builder.CreateStructGEP(interface_llvm_type, value, 1);
                builder.CreateStore(method_table, method_table_ptr);
            }

            return builder.CreateLoad(interface_llvm_type, value);
        }

        return visit_expression(*node.value);
    }

    llvm::Value* visit(HIRAssemblyExpression& node) override {
        return assembly.emit(node);
    }

    llvm::Value* visit(HIRCallExpression& node) override {
        return calls.emit(node);
    }

    llvm::Value* visit(HIRIndexExpression& node) override {
        auto* ptr = places.address(&node);
        if (ptr == nullptr) {
            return nullptr;
        }
        return builder.CreateLoad(lowerer.lower(node.type), ptr);
    }

    llvm::Value* visit(HIRMemberExpression& node) override {
        if (node.member == "value" && node.object != nullptr && node.object->type != nullptr &&
            node.object->type->is<EnumType>()) {
            auto* value = visit_expression(*node.object);
            return enum_tag(value);
        }

        auto* value = places.address(&node);
        if (value == nullptr) {
            return nullptr;
        }
        return builder.CreateLoad(lowerer.lower(node.type), value);
    }

    llvm::Value* visit(HIRAssignExpression& node) override {
        auto* value = visit_expression(*node.value);
        auto* target_value = places.address(node.target);
        if (value == nullptr || target_value == nullptr) {
            return nullptr;
        }

        builder.CreateStore(value, target_value);
        return value;
    }

    llvm::Value* visit(HIRStructLiteralExpression& node) override {
        return aggregates.emit(node);
    }

    llvm::Value* visit(HIREnumVariantExpression& node) override {
        return aggregates.emit(node);
    }

    llvm::Value* visit(HIRArrayLiteralExpression& node) override {
        return aggregates.emit(node);
    }

    llvm::Value* visit(HIRIfExpression& node) override {
        return flow.emit(node);
    }

    llvm::Value* visit(HIRWhenExpression& node) override {
        auto* subject_address = node.borrow_payloads && node.subject != nullptr ? places.address(node.subject) : nullptr;
        auto* subject_value = subject_address != nullptr ? builder.CreateLoad(lowerer.lower(node.subject->type), subject_address)
            : node.subject != nullptr ? visit_expression(*node.subject) : nullptr;
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
                          node.subject != nullptr ? node.subject->type : nullptr, subject_address, node.type,
                          merge_block, next_block, function, incoming_values, incoming_blocks);
        }

        if (!builder.GetInsertBlock()->hasTerminator()) {
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
        return flow.emit(node);
    }

    llvm::Value* visit(HIRVarDeclaration& node) override {
        if (node.is_global) {
            return scope.lookup(node.name);
        }

        auto* type = lowerer.lower(node.type);
        if (type == nullptr) {
            return nullptr;
        }

        auto* ptr = places.allocate(type);
        scope.set(node.name, ptr);

        if (node.initializer != nullptr) {
            auto* value = visit_expression(*node.initializer);
            if (value == nullptr) {
                return nullptr;
            }
            builder.CreateStore(value, ptr);
        }

        return ptr;
    }

    llvm::Value* visit(HIRFunctionDeclaration& node) override {
        auto* function = declarations.find_function(node.name);
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
        auto* last_value = visit_statement(*node.body);
        scope.exit();

        return_fallback(node.return_type, last_value);

        llvm::verifyFunction(*function);

        return function;
    }

  public:
    explicit Functions(llvm::LLVMContext& context, llvm::Module& module,
                       llvm::IRBuilder<>& builder, Types& lowerer, ValueScope& scope,
                       Dispatch& dispatch, Declarations& declarations)
        : context(context), module(module), builder(builder), lowerer(lowerer), scope(scope),
          dispatch(dispatch), declarations(declarations), places(builder, lowerer, scope, *this),
          conversions(builder, lowerer, places),
          calls(builder, lowerer, declarations, conversions, *this),
          assembly(builder, lowerer, places, *this), aggregates(builder, lowerer, places, *this),
          flow(builder, lowerer, scope, conversions, *this) {}

    void emit_definitions(const HIRProgram& program) {
        for (auto* statement : program.statements) {
            if (statement->as<HIRVarDeclaration>() != nullptr) {
                continue;
            }

            visit_statement(*statement);
        }
    }
};
