module;

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>
#include <string>
#include <vector>

export module zep.codegen.llvm.calls;

import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.lowerer.nodes;
import zep.codegen.llvm.types;
import zep.codegen.llvm.declarations;
import zep.codegen.llvm.values;
import zep.codegen.llvm.errors;

export class Dispatch {
  private:
    llvm::Module& module;
    llvm::IRBuilder<>& builder;

  public:
    Dispatch(llvm::Module& module, llvm::IRBuilder<>& builder)
        : module(module), builder(builder) {}

    llvm::GlobalVariable* get(const StructType* structure, const InterfaceType* interface,
                             const std::vector<HIRDispatchMethod>& methods) {
        if (structure == nullptr || interface == nullptr) {
            return nullptr;
        }

        const auto name = ".method_table." + structure->to_string() + "." + interface->to_string();

        if (auto* existing = module.getGlobalVariable(name); existing != nullptr) {
            return existing;
        }

        std::vector<llvm::Constant*> entries;
        entries.reserve(interface->methods.size());

        for (const auto& method : methods) {
            auto* function = module.getFunction(method.name);

            if (function == nullptr) {
                return nullptr;
            }

            entries.push_back(function);
        }

        auto* type = llvm::ArrayType::get(builder.getPtrTy(), entries.size());
        auto* initializer = llvm::ConstantArray::get(type, entries);

        return new llvm::GlobalVariable(module, type, true, llvm::GlobalValue::PrivateLinkage,
                                        initializer, name);
    }
};

export class Calls {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    Declarations& declarations;
    Conversions& conversions;
    Places& places;
    ErrorPaths& errors;
    HIRVisitor<llvm::Value*>& expressions;

  public:
    Calls(llvm::IRBuilder<>& builder, Types& types, Declarations& declarations,
          Conversions& conversions, Places& places, ErrorPaths& errors, HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), declarations(declarations), conversions(conversions),
          places(places), errors(errors), expressions(expressions) {}

    llvm::Value* emit(HIRCallExpression& node) {
        if (node.target == nullptr) {
            return nullptr;
        }

        const FunctionType* signature = nullptr;
        llvm::Value* callee = nullptr;
        llvm::Value* object = nullptr;
        auto abi = Abi::Type::Language;

        if (const auto* direct = node.target->as<HIRDirectCallTarget>(); direct != nullptr) {
            signature = direct->function_type;
            abi = direct->abi;
            callee = declarations.find_function(direct->emitted_name);

            if (callee == nullptr && !direct->emitted_name.empty()) {
                callee = declarations.declare_function(direct->emitted_name, signature,
                    llvm::GlobalValue::ExternalLinkage, abi);
            }
        } else if (const auto* indirect = node.target->as<HIRIndirectCallTarget>(); indirect != nullptr) {
            signature = indirect->function_type;
            callee = expressions.visit_expression(*indirect->callee);
        } else if (const auto* dispatch = node.target->as<HIRInterfaceCallTarget>(); dispatch != nullptr) {
            auto* receiver = !node.arguments.empty() ? node.arguments.front() : nullptr;

            if (const auto* unary = receiver != nullptr ? receiver->as<HIRUnaryExpression>() : nullptr;
                unary != nullptr && (unary->op == UnaryOperator::Type::AddressOf ||
                                     unary->op == UnaryOperator::Type::AddressOfMut)) {
                receiver = unary->operand;
            }

            if (receiver == nullptr) {
                return nullptr;
            }

            auto* value = expressions.visit_expression(*receiver);

            if (value == nullptr) {
                return nullptr;
            }

            object = builder.CreateExtractValue(value, {0});
            auto* table = builder.CreateExtractValue(value, {1});
            auto* index = llvm::ConstantInt::get(builder.getInt64Ty(), dispatch->slot);
            auto* entry = builder.CreateGEP(builder.getPtrTy(), table, index);
            callee = places.load(builder.getPtrTy(), entry);
            signature = dispatch->function_type;
        }

        if (signature == nullptr || callee == nullptr) {
            errors.diagnostics.add_error(node.span, "resolved call has no emitted callee or signature");

            return nullptr;
        }

        auto* function_type = types.lower_function(signature, abi);

        if (function_type == nullptr) {
            errors.diagnostics.add_error(node.span,
                "resolved call has an unresolved LLVM signature: " + signature->to_string());

            return nullptr;
        }

        if (const auto* function = llvm::dyn_cast<llvm::Function>(callee);
            function != nullptr && function->getFunctionType() != function_type) {
            errors.diagnostics.add_error(node.span,
                "resolved call signature differs from the emitted function: " + function->getName().str());

            return nullptr;
        }

        std::vector<llvm::Value*> arguments;
        arguments.reserve(node.arguments.size() + 2);
        llvm::Value* success = nullptr;
        llvm::Value* failure = nullptr;

        if (!signature->errors.empty()) {
            if (!signature->return_type->is<VoidType>() && !signature->return_type->is<NeverType>()) {
                success = places.allocate(types.lower(signature->return_type));
                arguments.push_back(success);
            }

            failure = errors.storage(*signature);

            if (failure == nullptr) {
                return nullptr;
            }

            arguments.push_back(failure);
        }

        for (auto index = std::size_t{0}; index < node.arguments.size(); ++index) {
            auto* value = index == 0 && object != nullptr ? object
                : expressions.visit_expression(*node.arguments[index]);

            if (builder.GetInsertBlock()->hasTerminator()) {
                return nullptr;
            }

            if (node.arguments[index]->type->is<VoidType>()) {
                continue;
            }

            if (value == nullptr) {
                return nullptr;
            }

            if (abi == Abi::Type::C && index < signature->parameters.size()) {
                value = conversions.abi(value, signature->parameters[index].type,
                                        Conversions::Form::Type::External);
            }

            arguments.push_back(value);
        }

        for (auto* transfer : node.transfers) {
            expressions.visit_statement(*transfer);
        }

        auto* result = builder.CreateCall(function_type, callee, arguments);

        if (!signature->errors.empty()) {
            return errors.complete(node, *signature, result, success, failure);
        }

        if (signature->return_type->is<NeverType>()) {
            result->addFnAttr(llvm::Attribute::NoReturn);
            builder.CreateUnreachable();

            return result;
        }

        return abi == Abi::Type::C ? conversions.abi(result, signature->return_type,
                                                     Conversions::Form::Type::Native)
                                  : result;
    }
};
