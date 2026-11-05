module;

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

export module zep.codegen.llvm.control_flow;

import zep.frontend.sema.types;
import zep.hir.node;
import zep.codegen.llvm.types;
import zep.codegen.llvm.values;

export class ControlFlow {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    ValueScope& scope;
    Conversions& conversions;
    HIRVisitor<llvm::Value*>& expressions;

  public:
    ControlFlow(llvm::IRBuilder<>& builder, Types& types, ValueScope& scope,
                Conversions& conversions, HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), scope(scope), conversions(conversions),
          expressions(expressions) {}

    llvm::Value* emit(HIRIfExpression& expression) {
        if (expression.condition == nullptr || expression.condition->type == nullptr) {
            return nullptr;
        }

        auto* condition = expressions.visit_expression(*expression.condition);

        if (condition == nullptr) {
            return nullptr;
        }

        auto& context = builder.getContext();
        auto* function = builder.GetInsertBlock()->getParent();
        auto* then_block = llvm::BasicBlock::Create(context, "", function);
        auto* else_block = llvm::BasicBlock::Create(context, "");
        auto* merge_block = llvm::BasicBlock::Create(context, "");

        builder.CreateCondBr(condition, then_block, else_block);
        builder.SetInsertPoint(then_block);

        auto* then_value = expressions.visit_statement(*expression.then_branch);
        const auto then_reaches_merge = !builder.GetInsertBlock()->hasTerminator();

        if (then_reaches_merge) {
            builder.CreateBr(merge_block);
        }

        then_block = builder.GetInsertBlock();
        function->insert(function->end(), else_block);
        builder.SetInsertPoint(else_block);

        llvm::Value* else_value = nullptr;

        if (expression.else_branch != nullptr) {
            else_value = expressions.visit_statement(*expression.else_branch);
        }

        const auto else_reaches_merge = !builder.GetInsertBlock()->hasTerminator();

        if (else_reaches_merge) {
            builder.CreateBr(merge_block);
        }

        else_block = builder.GetInsertBlock();
        function->insert(function->end(), merge_block);
        builder.SetInsertPoint(merge_block);

        if (expression.type == nullptr || expression.type->is<VoidType>()) {
            return nullptr;
        }

        auto* type = types.lower(expression.type);
        auto* result = builder.CreatePHI(
            type, static_cast<unsigned>(then_reaches_merge + else_reaches_merge));

        const auto incoming = [&](HIRStatement* branch, llvm::Value* value,
                                  llvm::BasicBlock* block, bool reachable) -> void {
            if (!reachable) {
                return;
            }

            if (branch == nullptr || branch->type == nullptr || branch->type->is<VoidType>()) {
                value = nullptr;
            } else if (value != nullptr && value->getType() != type) {
                value = conversions.cast(value, type, branch->type, expression.type);
            }

            result->addIncoming(value != nullptr ? value : llvm::UndefValue::get(type), block);
        };

        incoming(expression.then_branch, then_value, then_block, then_reaches_merge);
        incoming(expression.else_branch, else_value, else_block, else_reaches_merge);

        return result;
    }

    llvm::Value* emit(HIRLoopStatement& statement) {
        auto& context = builder.getContext();
        auto* function = builder.GetInsertBlock()->getParent();
        scope.enter();

        for (auto* initializer : statement.initializers) {
            expressions.visit_statement(*initializer);
        }

        auto* condition_block = llvm::BasicBlock::Create(context, "", function);
        auto* body_block = llvm::BasicBlock::Create(context, "", function);
        auto* step_block = llvm::BasicBlock::Create(context, "", function);
        auto* merge_block = llvm::BasicBlock::Create(context, "", function);
        builder.CreateBr(condition_block);
        builder.SetInsertPoint(condition_block);

        if (statement.condition == nullptr || statement.condition->type == nullptr) {
            return nullptr;
        }

        auto* condition = expressions.visit_expression(*statement.condition);

        if (condition == nullptr) {
            return nullptr;
        }

        builder.CreateCondBr(condition, body_block, merge_block);
        builder.SetInsertPoint(body_block);
        expressions.visit_statement(*statement.body);

        if (!builder.GetInsertBlock()->hasTerminator()) {
            builder.CreateBr(step_block);
        }

        builder.SetInsertPoint(step_block);

        if (statement.step != nullptr) {
            expressions.visit_expression(*statement.step);
        }

        if (!builder.GetInsertBlock()->hasTerminator()) {
            builder.CreateBr(condition_block);
        }

        builder.SetInsertPoint(merge_block);
        scope.exit();

        if (const auto* literal = statement.condition->as<HIRBooleanLiteral>();
            literal != nullptr && literal->value) {
            builder.CreateUnreachable();
        }

        return nullptr;
    }
};
