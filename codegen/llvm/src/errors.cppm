module;

#include <cstddef>
#include <cstdint>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Alignment.h>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.llvm.errors;

import zep.common.state;
import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.sema.types;
import zep.frontend.sema.types.errors;
import zep.hir.node;
import zep.codegen.llvm.types;
import zep.codegen.llvm.values;

export class ErrorPaths {
  private:
    class Handler {
      public:
        llvm::BasicBlock* block;
        llvm::Value* storage;

        Handler(llvm::BasicBlock* block, llvm::Value* storage) : block(block), storage(storage) {}
    };

    class Frame {
      public:
        const FunctionType* signature;
        llvm::Value* success;
        llvm::Value* error;
        std::unordered_map<std::string, Handler> handlers;

        Frame() : signature(nullptr), success(nullptr), error(nullptr), handlers() {}
    };

    llvm::IRBuilder<>& builder;
    Types& types;
    Places& places;
    HIRVisitor<llvm::Value*>& expressions;

    Frame frame;

  public:
    Diagnostics& diagnostics;

    ErrorPaths(llvm::IRBuilder<>& builder, Types& types, Places& places,
               HIRVisitor<llvm::Value*>& expressions, Diagnostics& diagnostics)
        : builder(builder), types(types), places(places), expressions(expressions), frame(),
          diagnostics(diagnostics) {}

    unsigned begin(const FunctionType* signature, llvm::Function* emitted) {
        frame = Frame();
        frame.signature = signature;

        auto index = 0u;

        if (!signature->errors.empty()) {
            if (!signature->return_type->is<VoidType>() && !signature->return_type->is<NeverType>()) {
                frame.success = emitted->getArg(index++);
            }

            frame.error = emitted->getArg(index++);
        }

        return index;
    }

    llvm::Value* success(llvm::Value* value, Conversions& conversions, const Type* source = nullptr) {
        const auto* signature = frame.signature;

        if (signature == nullptr) {
            diagnostics.add_error(Span(), "return has no active function signature");

            return nullptr;
        }

        if (value != nullptr && !signature->return_type->is<VoidType>() &&
            !signature->return_type->is<NeverType>() && value->getType() != types.lower(signature->return_type)) {
            value = conversions.cast(value, types.lower(signature->return_type), source, signature->return_type);
        }

        if (signature->errors.empty()) {
            return signature->return_type->is<VoidType>() ? builder.CreateRetVoid() : builder.CreateRet(value);
        }

        if (frame.success != nullptr) {
            if (value == nullptr) {
                diagnostics.add_error(Span(), "fallible function returned no success value");

                return nullptr;
            }

            places.store(value, frame.success);
        }

        return builder.CreateRet(builder.getInt32(0));
    }

    llvm::Value* storage(const FunctionType& signature) {
        const auto layout = ErrorSet::layout(signature.errors);

        if (layout.state != TypeLayout::Result::State::Concrete) {
            diagnostics.add_error(Span(), "checked error storage has no concrete layout");

            return nullptr;
        }

        auto* allocation = llvm::cast<llvm::AllocaInst>(places.allocate(
            llvm::ArrayType::get(builder.getInt8Ty(), layout.size)));
        allocation->setAlignment(llvm::Align(layout.alignment));

        return allocation;
    }

    llvm::Value* fail(llvm::Value* value, const HIRFailure& failure) {
        auto* destination = frame.error;
        llvm::BasicBlock* block = nullptr;

        if (!failure.target.empty()) {
            const auto handler = frame.handlers.find(failure.target);

            if (handler == frame.handlers.end()) {
                diagnostics.add_error(Span(), "checked failure has no active handler");

                return nullptr;
            }

            destination = handler->second.storage;
            block = handler->second.block;
        }

        if (destination == nullptr || value == nullptr) {
            diagnostics.add_error(Span(), "checked failure has no resolved destination or payload");

            return nullptr;
        }

        places.store(value, destination);

        for (auto* action : failure.cleanup) {
            expressions.visit_statement(*action);

            if (builder.GetInsertBlock()->hasTerminator()) {
                return nullptr;
            }
        }

        if (block != nullptr) {
            return builder.CreateBr(block);
        }

        for (auto index = std::size_t{0}; index < frame.signature->errors.size(); ++index) {
            if (frame.signature->errors[index]->same(failure.type)) {
                return builder.CreateRet(builder.getInt32(static_cast<std::uint32_t>(index + 1)));
            }
        }

        diagnostics.add_error(Span(), "checked error is not declared by the emitted function");

        return nullptr;
    }

    llvm::Value* complete(HIRCallExpression& node, const FunctionType& signature,
                          llvm::Value* status, llvm::Value* result, llvm::Value* payload) {
        auto* emitted = builder.GetInsertBlock()->getParent();
        auto* success_block = llvm::BasicBlock::Create(builder.getContext(), "success", emitted);
        auto* invalid_block = llvm::BasicBlock::Create(builder.getContext(), "invalid_error", emitted);
        auto* branch = builder.CreateSwitch(status, invalid_block,
            static_cast<unsigned>(signature.errors.size() + 1));
        branch->addCase(builder.getInt32(0), success_block);

        for (auto index = std::size_t{0}; index < signature.errors.size(); ++index) {
            const auto* type = signature.errors[index];
            const HIRFailure* failure = nullptr;

            for (const auto& candidate : node.failures) {
                if (candidate.type->same(type)) {
                    failure = &candidate;
                    break;
                }
            }

            if (failure == nullptr) {
                diagnostics.add_error(node.span, "fallible call has no checked failure edge");

                return nullptr;
            }

            auto* failed = llvm::BasicBlock::Create(builder.getContext(), "error", emitted);
            branch->addCase(builder.getInt32(static_cast<std::uint32_t>(index + 1)), failed);
            builder.SetInsertPoint(failed);
            fail(places.load(types.lower(type), payload), *failure);
        }

        builder.SetInsertPoint(invalid_block);
        builder.CreateUnreachable();
        builder.SetInsertPoint(success_block);

        if (signature.return_type->is<NeverType>()) {
            builder.CreateUnreachable();
            return nullptr;
        }

        return result != nullptr ? places.load(types.lower(signature.return_type), result) : nullptr;
    }

    llvm::Value* emit(HIRThrowExpression& node) {
        auto* value = expressions.visit_expression(*node.value);

        if (!builder.GetInsertBlock()->hasTerminator()) {
            fail(value, node.failure);
        }

        return nullptr;
    }

    llvm::Value* emit(HIRCatchExpression& node, ValueScope& scope, Conversions& conversions) {
        auto* emitted = builder.GetInsertBlock()->getParent();
        auto* merge = llvm::BasicBlock::Create(builder.getContext(), "handled", emitted);
        const auto has_value = !node.type->is<VoidType>() && !node.type->is<NeverType>();
        auto* result = has_value ? places.allocate(types.lower(node.type)) : nullptr;
        std::vector<Handler> targets;
        targets.reserve(node.arms.size());

        {
            StateGuard handler_guard(frame.handlers, frame.handlers);

            for (const auto& arm : node.arms) {
                auto* block = llvm::BasicBlock::Create(builder.getContext(), "catch", emitted);
                auto* storage = places.allocate(types.lower(arm.type));
                targets.emplace_back(block, storage);
                frame.handlers.emplace(arm.target, Handler(block, storage));
            }

            auto* value = expressions.visit_expression(*node.value);

            if (!builder.GetInsertBlock()->hasTerminator()) {
                if (result != nullptr) {
                    if (value == nullptr) {
                        diagnostics.add_error(node.span, "catch success path has no result");

                        return nullptr;
                    }

                    places.store(value, result);
                }

                builder.CreateBr(merge);
            }
        }

        for (auto index = std::size_t{0}; index < node.arms.size(); ++index) {
            auto& arm = node.arms[index];
            builder.SetInsertPoint(targets[index].block);
            scope.enter();
            scope.set(arm.name, targets[index].storage);
            auto* value = expressions.visit_statement(*arm.body);
            scope.exit();

            if (!builder.GetInsertBlock()->hasTerminator()) {
                if (result != nullptr) {
                    if (value == nullptr) {
                        diagnostics.add_error(node.span, "catch handler has no result");

                        return nullptr;
                    }

                    if (value->getType() != types.lower(node.type)) {
                        value = conversions.cast(value, types.lower(node.type), arm.body->type, node.type);
                    }

                    places.store(value, result);
                }

                builder.CreateBr(merge);
            }
        }

        builder.SetInsertPoint(merge);

        if (node.type->is<NeverType>()) {
            builder.CreateUnreachable();
            return nullptr;
        }

        return result != nullptr ? places.load(types.lower(node.type), result) : nullptr;
    }
};
