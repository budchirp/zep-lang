module;

#include <expected>
#include <filesystem>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

export module zep.codegen.llvm.backend;

import zep.codegen.api;
import zep.codegen.llvm.context;
import zep.codegen.llvm.type;
import zep.codegen.llvm.scope;
import zep.codegen.llvm.dispatch;
import zep.codegen.llvm.declarations;
import zep.codegen.llvm.emitter;
import zep.common.target;
import zep.frontend.sema.scope;
import zep.hir.node;
import zep.hir.program;

export class LLVMBackend : public CodegenBackend {
  public:
    CodegenFormat::Type format() const override { return CodegenFormat::Type::Object; }

    std::expected<void, std::string> generate(const HIRProgram& program,
                                              const std::filesystem::path& output,
                                              const CodegenOptions& options) override {
        try {
            LLVMEmissionContext context;
            context.initialize(options.target, options.optimization);

            TypeLowerer type_lowerer(*context.llvm_context);
            type_lowerer.declare(program.referenced_types);

            DispatchBuilder dispatch(*context.module, *context.builder);

            CodegenScope scope;
            DeclarationEmitter declarations(*context.module, type_lowerer, scope);
            declarations.emit_declarations(program);

            FunctionEmitter emitter(*context.llvm_context, *context.module, *context.builder,
                                    type_lowerer, scope, dispatch, declarations);
            emitter.emit_definitions(program);

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
