module;

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/Support/Error.h>
#include <stdexcept>
#include <vector>

export module zep.codegen.llvm.assembly;

import zep.frontend.kind;
import zep.hir.node;
import zep.codegen.llvm.types;
import zep.codegen.llvm.values;

export class Assembly {
  private:
    llvm::IRBuilder<>& builder;
    Types& types;
    Places& places;
    HIRVisitor<llvm::Value*>& expressions;

  public:
    Assembly(llvm::IRBuilder<>& builder, Types& types, Places& places,
                    HIRVisitor<llvm::Value*>& expressions)
        : builder(builder), types(types), places(places), expressions(expressions) {}

    llvm::Value* emit(HIRAssemblyExpression& node) {
        const auto& assembly = node.assembly;
        std::vector<llvm::Value*> values;
        std::vector<llvm::Value*> destinations;
        std::vector<llvm::Type*> output_types;
        std::vector<llvm::Value*> arguments;
        std::vector<llvm::Type*> argument_types;
        values.reserve(assembly.operands.size());
        destinations.reserve(assembly.operands.size());
        output_types.reserve(assembly.operands.size());
        arguments.reserve(assembly.operands.size());
        argument_types.reserve(assembly.operands.size());

        for (const auto& operand : assembly.operands) {
            auto* expression = node.arguments[operand.argument];

            if (operand.kind == AssemblyOperand::Kind::Type::Input) {
                destinations.push_back(nullptr);
                values.push_back(expressions.visit_expression(*expression));
                continue;
            }

            auto* destination = places.address(expression);
            auto* type = types.lower(expression->type);

            if (destination == nullptr || type == nullptr) {
                throw std::runtime_error("assembly output has no storage");
            }

            destinations.push_back(destination);
            output_types.push_back(type);

            if (operand.kind == AssemblyOperand::Kind::Type::InputOutput) {
                values.push_back(builder.CreateLoad(type, destination));
            } else {
                values.push_back(nullptr);
            }
        }

        for (auto index = std::size_t{0}; index < assembly.operands.size(); ++index) {
            if (assembly.operands[index].kind == AssemblyOperand::Kind::Type::Output) {
                continue;
            }

            auto* value = values[index];

            if (value == nullptr) {
                throw std::runtime_error("assembly operand did not produce a value");
            }

            arguments.push_back(value);
            argument_types.push_back(value->getType());
        }

        llvm::Type* result_type = builder.getVoidTy();

        if (output_types.size() == 1) {
            result_type = output_types.front();
        } else if (!output_types.empty()) {
            result_type = llvm::StructType::get(builder.getContext(), output_types);
        }

        auto* function_type = llvm::FunctionType::get(result_type, argument_types, false);

        if (auto error = llvm::InlineAsm::verify(function_type, assembly.constraints)) {
            throw std::runtime_error("invalid inline assembly constraints: " + llvm::toString(std::move(error)));
        }

        auto* function = llvm::InlineAsm::get(function_type, assembly.text, assembly.constraints,
            !assembly.pure, false, assembly.intel ? llvm::InlineAsm::AD_Intel : llvm::InlineAsm::AD_ATT);
        auto* result = builder.CreateCall(function, arguments);
        auto output = std::size_t{0};

        for (auto index = std::size_t{0}; index < assembly.operands.size(); ++index) {
            if (assembly.operands[index].kind == AssemblyOperand::Kind::Type::Input) {
                continue;
            }

            auto* value = output_types.size() == 1 ? static_cast<llvm::Value*>(result)
                : builder.CreateExtractValue(result, static_cast<unsigned>(output));

            builder.CreateStore(value, destinations[index]);
            ++output;
        }

        return result;
    }
};
