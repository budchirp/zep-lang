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
import zep.frontend.sema.scopes.scope;
import zep.hir.node;
import zep.codegen.llvm.types;
import zep.codegen.llvm.declarations;
import zep.codegen.llvm.values;

export class Dispatch {
  private:
    llvm::Module& module;
    llvm::IRBuilder<>& builder;

    llvm::Constant* resolve_method_pointer(const StructType* structure, const std::string& name) {
        if (structure == nullptr) {
            return llvm::ConstantPointerNull::get(builder.getPtrTy());
        }

        if (const auto* scope = Scope::members(structure); scope != nullptr) {
            const auto* overloads = scope->find_local_function_overloads(name);

            if (overloads != nullptr && !overloads->empty()) {
                const auto* symbol = overloads->front();

                if (auto* function = llvm::dyn_cast_or_null<llvm::Function>(
                        module.getNamedValue(symbol->name)); function != nullptr) {
                    return llvm::ConstantExpr::getBitCast(function, builder.getPtrTy());
                }
            }
        }

        return resolve_method_pointer(structure->base_type, name);
    }

  public:
    Dispatch(llvm::Module& module, llvm::IRBuilder<>& builder)
        : module(module), builder(builder) {}

    llvm::GlobalVariable* get(const StructType* structure, const InterfaceType* interface) {
        if (structure == nullptr || interface == nullptr) {
            return nullptr;
        }

        const auto name = ".method_table." + structure->name + "." + interface->name;

        if (auto* existing = module.getGlobalVariable(name); existing != nullptr) {
            return existing;
        }

        std::vector<llvm::Constant*> entries;
        entries.reserve(interface->methods.size());

        for (const auto& method : interface->methods) {
            entries.push_back(resolve_method_pointer(structure, method.name));
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
    HIRVisitor<llvm::Value*>& expressions;

  public:
    Calls(llvm::IRBuilder<>& builder, Types& types, Declarations& declarations,
          Conversions& conversions, HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), declarations(declarations), conversions(conversions),
          expressions(expressions) {}

    llvm::Value* emit(HIRCallExpression& node) {
        if (node.target == nullptr) {
            return nullptr;
        }

        const FunctionType* signature = nullptr;
        llvm::Value* callee = nullptr;
        llvm::Value* object = nullptr;
        auto abi = Abi::Type::Language;

        if (const auto* direct = node.target->as<HIRDirectCallTarget>(); direct != nullptr) {
            if (direct->function_symbol == nullptr) {
                return nullptr;
            }

            signature = direct->function_type;
            abi = direct->function_symbol->abi;
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
            callee = builder.CreateLoad(builder.getPtrTy(), entry);
            signature = dispatch->method_symbol->signature();
        }

        if (signature == nullptr || callee == nullptr) {
            return nullptr;
        }

        auto* function_type = types.lower_function(signature, abi);

        if (function_type == nullptr) {
            return nullptr;
        }

        if (const auto* function = llvm::dyn_cast<llvm::Function>(callee);
            function != nullptr && function->getFunctionType() != function_type) {
            return nullptr;
        }

        std::vector<llvm::Value*> arguments;
        arguments.reserve(node.arguments.size());

        for (auto index = std::size_t{0}; index < node.arguments.size(); ++index) {
            auto* value = index == 0 && object != nullptr ? object
                : expressions.visit_expression(*node.arguments[index]);

            if (value == nullptr) {
                return nullptr;
            }

            if (abi == Abi::Type::C && index < signature->parameters.size()) {
                value = conversions.abi(value, signature->parameters[index].type,
                                        Conversions::Form::Type::External);
            }

            arguments.push_back(value);
        }

        auto* result = builder.CreateCall(function_type, callee, arguments);

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
