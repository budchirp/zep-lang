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
import zep.common.diagnostic.collection;
import zep.frontend.sema.symbols;
import zep.frontend.sema.types;
import zep.hir.node;
import zep.hir.program;

export class LLVMBackend : public Backend {
  public:
    std::expected<void, std::string> generate(const HIRProgram& program,
                                              const std::filesystem::path& output,
                                              const EmitOptions& options) override {
        Target context;
        const auto initialized = context.initialize(options.target, options.optimization);

        if (!initialized.has_value()) {
            return std::unexpected(initialized.error());
        }

        Types types(*context.llvm_context);
        types.declare(program.referenced_types);

        for (const auto& entry : program.referenced_types) {
            const auto* type = entry.type;

            if (type == nullptr || (type->as_nominal() == nullptr && !type->is<ArrayType>())) {
                continue;
            }

            const auto layout = TypeLayout().resolve(type);

            if (layout.state != TypeLayout::Result::State::Concrete) {
                continue;
            }

            auto* lowered = types.lower(type);

            if (lowered == nullptr || !lowered->isSized()) {
                continue;
            }

            const auto& layout_data = context.module->getDataLayout();

            if (layout_data.getTypeAllocSize(lowered).getFixedValue() != layout.size ||
                layout_data.getABITypeAlign(lowered).value() != layout.alignment) {
                return std::unexpected("LLVM layout disagrees with semantic layout for '" + type->to_string() + "'");
            }
        }

        Dispatch dispatch(*context.module, *context.builder);
        ValueScope scope;
        Declarations declarations(*context.module, types, scope);
        declarations.emit_declarations(program);

        Diagnostics diagnostics;
        Functions functions(*context.llvm_context, *context.module, *context.builder,
                            types, scope, dispatch, declarations, diagnostics);
        functions.emit_definitions(program);

        if (diagnostics.has_errors()) {
            return std::unexpected(diagnostics.entries.front().message);
        }

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
    }
};
