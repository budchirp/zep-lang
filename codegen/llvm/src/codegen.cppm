module;

#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <memory>
#include <string>
#include <vector>

export module zep.codegen.llvm;

import zep.hir.node;
import zep.hir.node.program;
import zep.frontend.sema.type;
import zep.frontend.node;
import zep.codegen.driver;
import zep.codegen.llvm.context;
import zep.codegen.llvm.scope;
import zep.codegen.llvm.helper;
import zep.common.logger;

export class LLVMCodegen : public CodegenDriver, public HIRVisitor<llvm::Value*> {
  private:
    LLVMCodegenContext context;
    CodegenScope scope;
    LLVMCodegenHelper helper;

    llvm::Value* load(llvm::Type* type, llvm::Value* pointer) {
        return context.builder->CreateLoad(type, pointer);
    }

    llvm::Value* store(llvm::Value* value, llvm::Value* pointer) {
        return context.builder->CreateStore(value, pointer);
    }

    llvm::Value* allocate(llvm::Type* type) { return context.builder->CreateAlloca(type, nullptr); }

    llvm::Value* address_of(HIRExpression* node) {
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

            auto* object_address = address_of(member->object);

            if (object_address == nullptr) {
                auto* value = visit_expression(*member->object);
                object_address = allocate(helper.get_llvm_type(struct_type));
                store(value, object_address);
            }

            auto* llvm_struct_type = helper.get_llvm_type(struct_type);
            const auto& fields = struct_type->fields;
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].name == member->member) {
                    return context.builder->CreateStructGEP(llvm_struct_type, object_address,
                                                            static_cast<unsigned>(i));
                }
            }

            return nullptr;
        }

        if (auto* unary = node->as<HIRUnaryExpression>();
            unary != nullptr && unary->op == UnaryExpression::Operator::Type::Dereference) {
            return visit_expression(*unary->operand);
        }

        return nullptr;
    }

    llvm::Value* visit(HIRBlockStatement& node) override {
        scope.enter();

        llvm::Value* last = nullptr;
        for (auto* statement : node.statements) {
            last = visit_statement(*statement);
        }

        scope.exit();
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
        return context.builder->CreateGlobalString(node.value);
    }

    llvm::Value* visit(HIRBooleanLiteral& node) override {
        return llvm::ConstantInt::get(*context.llvm_context,
                                      llvm::APInt(1, node.value ? 1 : 0, false));
    }

    llvm::Value* visit(HIRIdentifierExpression& node) override {
        auto* allocation = scope.lookup(node.name);
        if (allocation == nullptr) {
            return nullptr;
        }

        return load(helper.get_llvm_type(node.type), allocation);
    }

    llvm::Value* visit(HIRBinaryExpression& node) override {
        auto* left = visit_expression(*node.left);
        if (left == nullptr) {
            return nullptr;
        }

        auto& builder = *context.builder;

        if (node.op == BinaryExpression::Operator::Type::As) {
            auto* target_type = helper.get_llvm_type(node.type);
            if (target_type == nullptr) {
                return nullptr;
            }

            auto* source_type = left->getType();

            if (source_type->isIntegerTy() && target_type->isIntegerTy()) {
                return builder.CreateIntCast(left, target_type, true);
            }
            if (source_type->isFloatingPointTy() && target_type->isFloatingPointTy()) {
                return builder.CreateFPCast(left, target_type);
            }
            if (source_type->isIntegerTy() && target_type->isFloatingPointTy()) {
                return builder.CreateSIToFP(left, target_type);
            }
            if (source_type->isFloatingPointTy() && target_type->isIntegerTy()) {
                return builder.CreateFPToSI(left, target_type);
            }
            if (source_type->isIntegerTy() && target_type->isPointerTy()) {
                return builder.CreateIntToPtr(left, target_type);
            }
            if (source_type->isPointerTy() && target_type->isIntegerTy()) {
                return builder.CreatePtrToInt(left, target_type);
            }

            return builder.CreateBitCast(left, target_type);
        }

        auto* right = visit_expression(*node.right);
        if (right == nullptr) {
            return nullptr;
        }

        switch (node.op) {
        case BinaryExpression::Operator::Type::Plus:
            return builder.CreateAdd(left, right);
        case BinaryExpression::Operator::Type::Minus:
            return builder.CreateSub(left, right);
        case BinaryExpression::Operator::Type::Asterisk:
            return builder.CreateMul(left, right);
        case BinaryExpression::Operator::Type::Divide:
            return builder.CreateSDiv(left, right);
        case BinaryExpression::Operator::Type::Equals:
            return builder.CreateICmpEQ(left, right);
        case BinaryExpression::Operator::Type::NotEquals:
            return builder.CreateICmpNE(left, right);
        case BinaryExpression::Operator::Type::LessThan:
            return builder.CreateICmpSLT(left, right);
        case BinaryExpression::Operator::Type::GreaterThan:
            return builder.CreateICmpSGT(left, right);
        case BinaryExpression::Operator::Type::LessEqual:
            return builder.CreateICmpSLE(left, right);
        case BinaryExpression::Operator::Type::GreaterEqual:
            return builder.CreateICmpSGE(left, right);
        default:
            return nullptr;
        }
    }

    llvm::Value* visit(HIRUnaryExpression& node) override {
        auto* operand = visit_expression(*node.operand);
        if (operand == nullptr) {
            return nullptr;
        }

        auto& builder = *context.builder;

        switch (node.op) {
        case UnaryExpression::Operator::Type::Minus:
            return builder.CreateNeg(operand);
        case UnaryExpression::Operator::Type::Not:
            return builder.CreateNot(operand);
        case UnaryExpression::Operator::Type::Dereference:
            return load(helper.get_llvm_type(node.type), operand);
        case UnaryExpression::Operator::Type::AddressOf:
            return address_of(node.operand);
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
        arguments.reserve(node.arguments.size());

        for (auto* arg : node.arguments) {
            auto* value = visit_expression(*arg);
            if (value == nullptr) {
                return nullptr;
            }

            arguments.push_back(value);
        }

        return context.builder->CreateCall(callee, arguments);
    }

    llvm::Value* visit(HIRIndexExpression& node) override {
        auto* object = visit_expression(*node.object);
        auto* index = visit_expression(*node.index);
        if (object == nullptr || index == nullptr) {
            return nullptr;
        }

        auto* element_type = helper.get_llvm_type(node.type);

        if (object->getType()->isPointerTy()) {
            auto* pointer = context.builder->CreateGEP(element_type, object, {index});
            return load(element_type, pointer);
        }

        return nullptr;
    }

    llvm::Value* visit(HIRMemberExpression& node) override {
        auto* address = address_of(&node);
        if (address == nullptr) {
            return nullptr;
        }

        return load(helper.get_llvm_type(node.type), address);
    }

    llvm::Value* visit(HIRAssignExpression& node) override {
        auto* value = visit_expression(*node.value);
        auto* address = address_of(node.target);

        if (value == nullptr || address == nullptr) {
            return nullptr;
        }

        return store(value, address);
    }

    llvm::Value* visit(HIRStructLiteralExpression& node) override {
        const auto* struct_type = node.type->as<StructType>();
        auto* llvm_type = helper.get_llvm_type(struct_type);
        auto* allocation = allocate(llvm_type);

        for (const auto& field : node.fields) {
            auto* value = visit_expression(*field.value);
            if (value == nullptr) {
                return nullptr;
            }

            const auto& fields = struct_type->fields;
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].name == field.name) {
                    auto* field_ptr = context.builder->CreateStructGEP(llvm_type, allocation,
                                                                       static_cast<unsigned>(i));
                    store(value, field_ptr);
                    break;
                }
            }
        }

        return load(llvm_type, allocation);
    }

    llvm::Value* visit(HIRIfExpression& node) override {
        auto* condition = visit_expression(*node.condition);
        if (condition == nullptr) {
            return nullptr;
        }

        auto& builder = *context.builder;
        auto* function = builder.GetInsertBlock()->getParent();

        auto* then_block = llvm::BasicBlock::Create(*context.llvm_context, "", function);
        auto* else_block = llvm::BasicBlock::Create(*context.llvm_context, "");
        auto* merge_block = llvm::BasicBlock::Create(*context.llvm_context, "");

        builder.CreateCondBr(condition, then_block, else_block);

        builder.SetInsertPoint(then_block);
        auto* then_value = visit_statement(*node.then_branch);
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
        then_block = builder.GetInsertBlock();

        function->insert(function->end(), else_block);
        builder.SetInsertPoint(else_block);
        llvm::Value* else_value = nullptr;
        if (node.else_branch != nullptr) {
            else_value = visit_statement(*node.else_branch);
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
        return visit_expression(*node.expression);
    }

    llvm::Value* visit(HIRReturnStatement& node) override {
        if (node.value == nullptr) {
            return context.builder->CreateRetVoid();
        }

        auto* value = visit_expression(*node.value);
        if (value == nullptr) {
            return nullptr;
        }

        return context.builder->CreateRet(value);
    }

    llvm::Value* visit(HIRVarDeclaration& node) override {
        auto* type = helper.get_llvm_type(node.type);
        if (type == nullptr) {
            return nullptr;
        }

        auto* allocation = allocate(type);
        scope.set(node.name, allocation);

        if (node.initializer != nullptr) {
            auto* value = visit_expression(*node.initializer);
            if (value == nullptr) {
                return nullptr;
            }

            store(value, allocation);
        }

        return allocation;
    }

    llvm::Value* visit(HIRFunctionDeclaration& node) override {
        auto* function = context.module->getFunction(node.name);
        if (function == nullptr || node.body == nullptr) {
            return function;
        }

        auto& builder = *context.builder;
        auto* entry = llvm::BasicBlock::Create(*context.llvm_context, "", function);
        builder.SetInsertPoint(entry);

        scope.enter();

        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            auto& param = node.parameters[i];
            auto* argument = function->getArg(static_cast<unsigned>(i));
            argument->setName(param.name);

            auto* allocation = allocate(helper.get_llvm_type(param.type));
            store(argument, allocation);
            scope.set(param.name, allocation);
        }

        visit_statement(*node.body);
        scope.exit();

        if (node.return_type->is<VoidType>() &&
            builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateRetVoid();
        }

        llvm::verifyFunction(*function);
        return function;
    }

    void initialize() {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();

        llvm::Triple target_triple(llvm::sys::getDefaultTargetTriple());
        context.module->setTargetTriple(target_triple);

        std::string error;
        const auto* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
        if (target == nullptr) {
            throw std::runtime_error("failed to lookup target: " + error);
        }

        llvm::TargetOptions opt;

        auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
        context.target_machine.reset(
            target->createTargetMachine(target_triple, "generic", "", opt, rm));
        if (context.target_machine == nullptr) {
            throw std::runtime_error("failed to create target machine");
        }

        context.module->setDataLayout(context.target_machine->createDataLayout());
    }

  public:
    LLVMCodegen() : helper(context) {}

    ~LLVMCodegen() override { llvm::llvm_shutdown(); }

    void generate(HIRProgram& program, const std::string& output_path) override {
        helper.declare_types(*program.global_scope);
        helper.declare_functions(*program.global_scope);

        for (auto* statement : program.statements) {
            visit_statement(*statement);
        }

        context.module->print(llvm::errs(), nullptr);

        initialize();

        std::error_code ec;

        llvm::raw_fd_ostream output(output_path, ec, llvm::sys::fs::OF_None);
        if (ec) {
            throw std::runtime_error("could not open output file: " + ec.message());
        }

        llvm::legacy::PassManager pass;
        if (context.target_machine->addPassesToEmitFile(pass, output, nullptr,
                                                        llvm::CodeGenFileType::ObjectFile)) {
            throw std::runtime_error("target machine can't emit a file of this type");
        }

        pass.run(*context.module);
        output.flush();
    }
};
