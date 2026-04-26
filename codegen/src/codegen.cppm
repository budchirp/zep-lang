module;

#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <string>
#include <vector>

export module zep.codegen;

import zep.hir.node;
import zep.hir.node.program;
import zep.frontend.sema.type;
import zep.codegen.context;
import zep.codegen.helper;
import zep.common.logger;

export class Codegen : public HIRVisitor<llvm::Value*> {
  private:
    CodegenContext context;
    CodegenHelper helper;

  public:
    explicit Codegen() : context(), helper(context) {}

    void generate(HIRProgram& program) {
        helper.declare_types(*program.global_scope);
        helper.declare_functions(*program.global_scope);

        for (auto* statement : program.statements) {
            statement->accept(*this);
        }
    }

    int get_member_index(const StructType* struct_type, const std::string& member) {
        for (std::size_t i = 0; i < struct_type->fields.size(); ++i) {
            if (struct_type->fields[i].name == member) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    llvm::Value* get_address(HIRExpression* node) {
        if (node == nullptr) {
            return nullptr;
        }

        if (auto* identifier = node->as<HIRIdentifierExpression>(); identifier != nullptr) {
            return context.lookup(identifier->name);
        }

        if (auto* member = node->as<HIRMemberExpression>(); member != nullptr) {
            auto* type = member->object->type;
            if (type == nullptr) {
                return nullptr;
            }

            const auto* struct_type = type->as<StructType>();
            if (struct_type == nullptr) {
                return nullptr;
            }

            auto* address = get_address(member->object);

            if (address == nullptr) {
                auto* value = member->object->accept(*this);
                address = context.builder.CreateAlloca(helper.get_llvm_type(struct_type));
                context.builder.CreateStore(value, address);
            }

            auto index = get_member_index(struct_type, member->member);

            return context.builder.CreateStructGEP(helper.get_llvm_type(struct_type), address,
                                                   static_cast<unsigned>(index));
        }

        if (auto* unary = node->as<HIRUnaryExpression>();
            unary != nullptr && unary->op == HIRUnaryExpression::Operator::Type::Dereference) {
            return unary->operand->accept(*this);
        }

        return nullptr;
    }

    llvm::Value* visit(HIRBlockStatement& node) override {
        context.enter_scope();

        llvm::Value* last = nullptr;

        for (auto* statement : node.statements) {
            last = statement->accept(*this);
        }

        context.exit_scope();

        return last;
    }

    llvm::Value* visit(HIRNumberLiteral& node) override {
        auto* llvm_type = helper.get_llvm_type(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }

        return llvm::ConstantInt::get(
            llvm_type, llvm::APInt(llvm_type->getIntegerBitWidth(),
                                   static_cast<std::uint64_t>(std::stoll(node.value)), true));
    }

    llvm::Value* visit(HIRFloatLiteral& node) override {
        auto* llvm_type = helper.get_llvm_type(node.type);
        if (llvm_type == nullptr) {
            return nullptr;
        }

        return llvm::ConstantFP::get(llvm_type, llvm::APFloat(std::stod(node.value)));
    }

    llvm::Value* visit(HIRStringLiteral& node) override {
        return context.builder.CreateGlobalString(node.value);
    }

    llvm::Value* visit(HIRBooleanLiteral& node) override {
        return llvm::ConstantInt::get(context.llvm_context,
                                      llvm::APInt(1, node.value ? 1 : 0, false));
    }

    llvm::Value* visit(HIRIdentifierExpression& node) override {
        auto* allocation = context.lookup(node.name);
        if (allocation == nullptr) {
            return nullptr;
        }

        return context.builder.CreateLoad(helper.get_llvm_type(node.type), allocation);
    }

    llvm::Value* visit(HIRBinaryExpression& node) override {
        auto* left = node.left->accept(*this);
        if (left == nullptr) {
            return nullptr;
        }

        if (node.op == HIRBinaryExpression::Operator::Type::As) {
            auto* target_llvm_type = helper.get_llvm_type(node.type);
            if (target_llvm_type == nullptr) {
                return nullptr;
            }

            auto& builder = context.builder;
            auto* source_type = left->getType();

            if (source_type->isIntegerTy() && target_llvm_type->isIntegerTy()) {
                return builder.CreateIntCast(left, target_llvm_type, true);
            }

            if (source_type->isFloatingPointTy() && target_llvm_type->isFloatingPointTy()) {
                return builder.CreateFPCast(left, target_llvm_type);
            }

            if (source_type->isIntegerTy() && target_llvm_type->isFloatingPointTy()) {
                return builder.CreateSIToFP(left, target_llvm_type);
            }

            if (source_type->isFloatingPointTy() && target_llvm_type->isIntegerTy()) {
                return builder.CreateFPToSI(left, target_llvm_type);
            }

            if (source_type->isIntegerTy() && target_llvm_type->isPointerTy()) {
                return builder.CreateIntToPtr(left, target_llvm_type);
            }

            if (source_type->isPointerTy() && target_llvm_type->isIntegerTy()) {
                return builder.CreatePtrToInt(left, target_llvm_type);
            }

            return builder.CreateBitCast(left, target_llvm_type);
        }

        auto* right = node.right->accept(*this);
        if (right == nullptr) {
            return nullptr;
        }

        auto& builder = context.builder;

        switch (node.op) {
        case HIRBinaryExpression::Operator::Type::Plus:
            return builder.CreateAdd(left, right);
        case HIRBinaryExpression::Operator::Type::Minus:
            return builder.CreateSub(left, right);
        case HIRBinaryExpression::Operator::Type::Asterisk:
            return builder.CreateMul(left, right);
        case HIRBinaryExpression::Operator::Type::Divide:
            return builder.CreateSDiv(left, right);
        case HIRBinaryExpression::Operator::Type::Equals:
            return builder.CreateICmpEQ(left, right);
        case HIRBinaryExpression::Operator::Type::NotEquals:
            return builder.CreateICmpNE(left, right);
        case HIRBinaryExpression::Operator::Type::LessThan:
            return builder.CreateICmpSLT(left, right);
        case HIRBinaryExpression::Operator::Type::GreaterThan:
            return builder.CreateICmpSGT(left, right);
        case HIRBinaryExpression::Operator::Type::LessEqual:
            return builder.CreateICmpSLE(left, right);
        case HIRBinaryExpression::Operator::Type::GreaterEqual:
            return builder.CreateICmpSGE(left, right);
        default:
            return nullptr;
        }
    }

    llvm::Value* visit(HIRUnaryExpression& node) override {
        auto* operand = node.operand->accept(*this);
        if (operand == nullptr) {
            return nullptr;
        }

        auto& builder = context.builder;

        switch (node.op) {
        case HIRUnaryExpression::Operator::Type::Minus:
            return builder.CreateNeg(operand);
        case HIRUnaryExpression::Operator::Type::Not:
            return builder.CreateNot(operand);
        case HIRUnaryExpression::Operator::Type::Dereference:
            return builder.CreateLoad(helper.get_llvm_type(node.type), operand);
        case HIRUnaryExpression::Operator::Type::AddressOf:
            return get_address(node.operand);
        default:
            return nullptr;
        }
    }

    llvm::Value* visit(HIRCallExpression& node) override {
        auto* callee_id = node.callee->as<HIRIdentifierExpression>();
        if (callee_id == nullptr) {
            return nullptr;
        }

        auto* callee = context.module->getFunction(callee_id->name);
        if (callee == nullptr) {
            return nullptr;
        }

        std::vector<llvm::Value*> arguments;
        for (auto* arg : node.arguments) {
            auto* value = arg->accept(*this);
            if (value == nullptr) {
                return nullptr;
            }

            arguments.push_back(value);
        }

        return context.builder.CreateCall(callee, arguments);
    }
    llvm::Value* visit(HIRIndexExpression& node) override {
        auto* object = node.object->accept(*this);
        auto* index = node.index->accept(*this);
        if (object == nullptr || index == nullptr) {
            return nullptr;
        }

        auto& builder = context.builder;
        auto* element_type = helper.get_llvm_type(node.type);

        if (object->getType()->isPointerTy()) {
            auto* pointer = builder.CreateGEP(element_type, object, {index});
            return builder.CreateLoad(element_type, pointer);
        }

        return nullptr;
    }
    llvm::Value* visit(HIRMemberExpression& node) override {
        auto* address = get_address(&node);
        if (address == nullptr) {
            return nullptr;
        }

        return context.builder.CreateLoad(helper.get_llvm_type(node.type), address);
    }
    llvm::Value* visit(HIRAssignExpression& node) override {
        auto* value = node.value->accept(*this);
        auto* address = get_address(node.target);

        if (address == nullptr) {
            return nullptr;
        }

        if (value == nullptr) {
            return nullptr;
        }

        return context.builder.CreateStore(value, address);
    }
    llvm::Value* visit(HIRStructLiteralExpression& node) override {
        auto* struct_type = node.type->as<StructType>();
        auto* llvm_type = helper.get_llvm_type(struct_type);
        auto& builder = context.builder;

        auto* allocation = builder.CreateAlloca(llvm_type, nullptr);

        for (const auto& field : node.fields) {
            int index = get_member_index(struct_type, field.name);
            auto* value = field.value->accept(*this);
            if (value == nullptr) {
                return nullptr;
            }

            auto* field_pointer =
                builder.CreateStructGEP(llvm_type, allocation, static_cast<unsigned>(index));
            builder.CreateStore(value, field_pointer);
        }

        return builder.CreateLoad(llvm_type, allocation);
    }
    llvm::Value* visit(HIRIfExpression& node) override {
        auto* condition = node.condition->accept(*this);
        if (condition == nullptr) {
            return nullptr;
        }

        auto& builder = context.builder;
        auto* function = builder.GetInsertBlock()->getParent();

        auto* then_block = llvm::BasicBlock::Create(context.llvm_context, "", function);
        auto* else_block = llvm::BasicBlock::Create(context.llvm_context, "");
        auto* merge_block = llvm::BasicBlock::Create(context.llvm_context, "");

        builder.CreateCondBr(condition, then_block, else_block);

        builder.SetInsertPoint(then_block);
        auto* then_value = node.then_branch->accept(*this);
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
        then_block = builder.GetInsertBlock();

        function->insert(function->end(), else_block);
        builder.SetInsertPoint(else_block);
        llvm::Value* else_value = nullptr;
        if (node.else_branch != nullptr) {
            else_value = node.else_branch->accept(*this);
        }
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
        else_block = builder.GetInsertBlock();

        function->insert(function->end(), merge_block);
        builder.SetInsertPoint(merge_block);

        if (node.type != nullptr && !node.type->is<VoidType>()) {
            auto* phi = builder.CreatePHI(helper.get_llvm_type(node.type), 2);
            phi->addIncoming(then_value, then_block);
            phi->addIncoming(else_value, else_block);
            return phi;
        }

        return nullptr;
    }
    llvm::Value* visit(HIRTypeExpression& node [[maybe_unused]]) override { return nullptr; }

    llvm::Value* visit(HIRExpressionStatement& node) override {
        return node.expression->accept(*this);
    }

    llvm::Value* visit(HIRReturnStatement& node) override {
        if (node.value == nullptr) {
            return context.builder.CreateRetVoid();
        }

        auto* value = node.value->accept(*this);
        if (value == nullptr) {
            return nullptr;
        }

        return context.builder.CreateRet(value);
    }

    llvm::Value* visit(HIRVarDeclaration& node) override {
        auto* type = helper.get_llvm_type(node.type);
        if (type == nullptr) {
            return nullptr;
        }

        auto& builder = context.builder;
        auto* allocation = builder.CreateAlloca(type, nullptr);
        context.set(node.name, allocation);

        if (node.initializer != nullptr) {
            auto* value = node.initializer->accept(*this);
            if (value == nullptr) {
                return nullptr;
            }

            builder.CreateStore(value, allocation);
        }

        return allocation;
    }

    llvm::Value* visit(HIRFunctionDeclaration& node) override {
        auto* function = context.module->getFunction(node.name);
        if (function == nullptr) {
            return nullptr;
        }

        if (node.body == nullptr) {
            return function;
        }

        auto& builder = context.builder;
        auto* entry = llvm::BasicBlock::Create(context.llvm_context, "", function);
        builder.SetInsertPoint(entry);

        context.enter_scope();

        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            auto& param = node.parameters[i];
            auto* argument = function->getArg(static_cast<unsigned>(i));
            argument->setName(param.name);

            auto* allocation = builder.CreateAlloca(helper.get_llvm_type(param.type), nullptr);
            builder.CreateStore(argument, allocation);
            context.set(param.name, allocation);
        }

        node.body->accept(*this);
        context.exit_scope();

        if (node.return_type->is<VoidType>() &&
            builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateRetVoid();
        }

        llvm::verifyFunction(*function);
        return function;
    }
};
