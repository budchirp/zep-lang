module;

#include <expected>
#include <filesystem>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <string>
#include <vector>

export module zep.codegen.llvm.backend;

import zep.codegen.api;
import zep.codegen.llvm.target_context;
import zep.codegen.llvm.types;
import zep.codegen.llvm.values;
import zep.codegen.llvm.calls;
import zep.codegen.llvm.declarations;
import zep.codegen.llvm.functions;
import zep.common.target;
import zep.frontend.sema.symbols;
import zep.frontend.sema.types;
import zep.hir.node;
import zep.hir.program;

export class LLVMBackend : public Backend {
  public:
    std::expected<void, std::string> generate(const HIRProgram& program,
                                              const std::filesystem::path& output,
                                              const EmitOptions& options) override {
        try {
            Target context;
            context.initialize(options.target, options.optimization);

            Types type_lowerer(*context.llvm_context);
            type_lowerer.declare(program.referenced_types);

            for (const auto* symbol : program.referenced_types) {
                const auto* type = symbol != nullptr ? symbol->type : nullptr;

                if (type == nullptr || (type->as_nominal() == nullptr && !type->is<ArrayType>())) {
                    continue;
                }

                const auto layout = TypeLayout().resolve(type);

                if (layout.state != TypeLayout::Result::State::Concrete) {
                    continue;
                }

                auto* lowered = type_lowerer.lower(type);

                if (lowered == nullptr || !lowered->isSized()) {
                    continue;
                }

                const auto& data_layout = context.module->getDataLayout();
                if (data_layout.getTypeAllocSize(lowered).getFixedValue() != layout.size ||
                    data_layout.getABITypeAlign(lowered).value() != layout.alignment) {
                    return std::unexpected("LLVM layout disagrees with semantic layout for '" + type->to_string() + "'");
                }
            }

            Dispatch dispatch(*context.module, *context.builder);

            ValueScope scope;
            Declarations declarations(*context.module, type_lowerer, scope);
            declarations.emit_declarations(program);

            Functions emitter(*context.llvm_context, *context.module, *context.builder,
                                    type_lowerer, scope, dispatch, declarations);
            emitter.emit_definitions(program);

            for (auto* statement : program.statements) {
                auto* function = statement->as<HIRFunctionDeclaration>();
                if (function == nullptr || !function->is_global_destructor) {
                    continue;
                }

                auto* destructor = context.module->getFunction(function->name);
                if (destructor != nullptr) {
                    llvm::appendToGlobalDtors(*context.module, destructor, 65535);
                }
            }

            std::string verification_message;
            llvm::raw_string_ostream verification_stream(verification_message);
            if (llvm::verifyModule(*context.module, &verification_stream)) {
                return std::unexpected("LLVM verification failed: " + verification_message);
            }

            context.optimize_module(options.optimization);

            if (options.debug_output == DebugOutput::Type::IR) {
                context.module->print(llvm::errs(), nullptr);
            }

            return context.emit_object(output);
        } catch (const std::exception& exception) {
            return std::unexpected(exception.what());
        }
    }
};
