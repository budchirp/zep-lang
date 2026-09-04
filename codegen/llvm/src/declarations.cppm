module;

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.codegen.llvm.declarations;

import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.kind;
import zep.codegen.llvm.type;
import zep.codegen.llvm.scope;
import zep.hir.node;
import zep.hir.program;

export class DeclarationEmitter {
  private:
    llvm::Module& module;
    TypeLowerer& type_lowerer;
    CodegenScope& scope;
    std::unordered_map<const FunctionSymbol*, llvm::Function*> function_values;

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

  public:
    explicit DeclarationEmitter(llvm::Module& module, TypeLowerer& type_lowerer,
                                CodegenScope& scope)
        : module(module), type_lowerer(type_lowerer), scope(scope) {}

    llvm::Function* declare_function(const std::string& name, const FunctionType* function_type,
                                     llvm::GlobalValue::LinkageTypes linkage,
                                     Abi::Type abi = Abi::Type::Language) {
        if (function_type == nullptr) {
            return nullptr;
        }

        auto* llvm_function_type = type_lowerer.lower_function(function_type, abi);
        if (llvm_function_type == nullptr) {
            return nullptr;
        }

        if (auto* existing = module.getNamedValue(name); existing != nullptr) {
            auto* function = llvm::dyn_cast<llvm::Function>(existing);
            return function != nullptr && function->getFunctionType() == llvm_function_type
                       ? function
                       : nullptr;
        }

        auto* function = llvm::Function::Create(llvm_function_type, linkage, name, module);
        if (function_type->return_type->is<NeverType>()) {
            function->addFnAttr(llvm::Attribute::NoReturn);
        }

        return function;
    }

    llvm::Function* declare_function(const HIRFunctionDeclaration& node) {
        const auto* function_type = node.type->as<FunctionType>();
        auto* function =
            declare_function(node.name, function_type, llvm_linkage(node.linkage), node.abi);
        if (node.function_symbol != nullptr && function != nullptr) {
            function_values[node.function_symbol] = function;
        }
        return function;
    }

    llvm::Function* find_function(const FunctionSymbol* symbol, const std::string& name) {
        if (symbol != nullptr) {
            if (auto iterator = function_values.find(symbol); iterator != function_values.end()) {
                return iterator->second;
            }
        }

        return llvm::dyn_cast_or_null<llvm::Function>(module.getNamedValue(name));
    }

    llvm::Constant* emit_constant(HIRExpression& expr, llvm::Type* expected_type) {
        if (auto* number = expr.as<HIRNumberLiteral>(); number != nullptr) {
            return llvm::ConstantInt::get(
                expected_type, static_cast<std::uint64_t>(std::stoull(number->value, nullptr, 0)));
        }

        if (auto* null_literal = expr.as<HIRNullLiteral>(); null_literal != nullptr) {
            return llvm::Constant::getNullValue(expected_type);
        }

        if (auto* boolean_literal = expr.as<HIRBooleanLiteral>(); boolean_literal != nullptr) {
            return llvm::ConstantInt::get(expected_type, boolean_literal->value ? 1 : 0);
        }

        if (auto* struct_literal = expr.as<HIRStructLiteralExpression>();
            struct_literal != nullptr) {
            auto* struct_type = llvm::dyn_cast<llvm::StructType>(expected_type);
            if (struct_type == nullptr) {
                return nullptr;
            }

            std::vector<llvm::Constant*> field_constants;
            field_constants.reserve(struct_literal->fields.size());

            for (const auto& field : struct_literal->fields) {
                auto* field_type =
                    struct_type->getElementType(static_cast<unsigned>(field_constants.size()));
                auto* field_constant = emit_constant(*field.value, field_type);
                if (field_constant == nullptr) {
                    return nullptr;
                }

                field_constants.push_back(field_constant);
            }

            return llvm::ConstantStruct::get(struct_type, field_constants);
        }

        if (auto* array_literal = expr.as<HIRArrayLiteralExpression>(); array_literal != nullptr) {
            auto* array_type = llvm::dyn_cast<llvm::ArrayType>(expected_type);
            if (array_type == nullptr) {
                return nullptr;
            }

            auto* element_type = array_type->getElementType();
            std::vector<llvm::Constant*> element_constants;
            element_constants.reserve(array_literal->elements.size());

            for (auto* element : array_literal->elements) {
                auto* element_constant = emit_constant(*element, element_type);
                if (element_constant == nullptr) {
                    return nullptr;
                }

                element_constants.push_back(element_constant);
            }

            return llvm::ConstantArray::get(array_type, element_constants);
        }

        return nullptr;
    }

    void emit_global_variable(HIRVarDeclaration& node) {
        if (auto* existing = module.getGlobalVariable(node.name); existing != nullptr) {
            scope.set(node.name, existing);
            return;
        }

        auto* type = type_lowerer.lower(node.type);
        if (type == nullptr) {
            return;
        }

        auto linkage = node.linkage == Linkage::Type::External ? llvm::GlobalValue::ExternalLinkage
                                                               : llvm::GlobalValue::InternalLinkage;
        auto* global = new llvm::GlobalVariable(module, type, false, linkage, nullptr, node.name);

        for (const auto& attribute : node.attributes) {
            if (attribute.name == "section" && !attribute.arguments.empty()) {
                global->setSection(attribute.arguments[0]);
            }
            if (attribute.name == "align" && !attribute.arguments.empty()) {
                auto alignment = std::stoull(attribute.arguments[0]);
                global->setAlignment(llvm::MaybeAlign(alignment));
            }
        }

        if (node.initializer != nullptr) {
            auto* constant = emit_constant(*node.initializer, type);
            if (constant != nullptr) {
                global->setInitializer(constant);
            } else {
                global->setInitializer(llvm::Constant::getNullValue(type));
            }
        } else if (node.linkage != Linkage::Type::External) {
            global->setInitializer(llvm::Constant::getNullValue(type));
        }

        scope.set(node.name, global);
    }

    void emit_declarations(const HIRProgram& program) {
        function_values.clear();

        for (auto* statement : program.statements) {
            if (auto* function = statement->as<HIRFunctionDeclaration>(); function != nullptr) {
                declare_function(*function);
            } else if (auto* var = statement->as<HIRVarDeclaration>();
                       var != nullptr && var->is_global) {
                emit_global_variable(*var);
            }
        }
    }
};
