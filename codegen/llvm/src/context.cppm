module;

#include <expected>
#include <filesystem>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <stdexcept>
#include <string>

export module zep.codegen.llvm.context;

import zep.common.target;
import zep.codegen.api;

export class LLVMEmissionContext {
  public:
    std::unique_ptr<llvm::LLVMContext> llvm_context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::TargetMachine> target_machine;

    LLVMEmissionContext()
        : llvm_context(std::make_unique<llvm::LLVMContext>()),
          builder(std::make_unique<llvm::IRBuilder<>>(*llvm_context)),
          module(std::make_unique<llvm::Module>("zep", *llvm_context)) {}

    static llvm::CodeGenOptLevel codegen_level(OptimizationLevel::Type level) {
        switch (level) {
        case OptimizationLevel::Type::O0:
            return llvm::CodeGenOptLevel::None;
        case OptimizationLevel::Type::O1:
            return llvm::CodeGenOptLevel::Less;
        case OptimizationLevel::Type::O2:
            return llvm::CodeGenOptLevel::Default;
        case OptimizationLevel::Type::O3:
            return llvm::CodeGenOptLevel::Aggressive;
        }

        return llvm::CodeGenOptLevel::None;
    }

    void optimize_module(OptimizationLevel::Type optimization_level) const {
        llvm::OptimizationLevel level;
        switch (optimization_level) {
        case OptimizationLevel::Type::O0:
            level = llvm::OptimizationLevel::O0;
            break;
        case OptimizationLevel::Type::O1:
            level = llvm::OptimizationLevel::O1;
            break;
        case OptimizationLevel::Type::O2:
            level = llvm::OptimizationLevel::O2;
            break;
        case OptimizationLevel::Type::O3:
            level = llvm::OptimizationLevel::O3;
            break;
        default:
            level = llvm::OptimizationLevel::O0;
        }

        llvm::LoopAnalysisManager loop_analysis_manager;
        llvm::FunctionAnalysisManager function_analysis_manager;
        llvm::CGSCCAnalysisManager cgscc_analysis_manager;
        llvm::ModuleAnalysisManager module_analysis_manager;
        llvm::PassBuilder pass_builder(target_machine.get());

        pass_builder.registerModuleAnalyses(module_analysis_manager);
        pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
        pass_builder.registerFunctionAnalyses(function_analysis_manager);
        pass_builder.registerLoopAnalyses(loop_analysis_manager);
        pass_builder.crossRegisterProxies(loop_analysis_manager, function_analysis_manager,
                                          cgscc_analysis_manager, module_analysis_manager);

        llvm::ModulePassManager module_pass_manager;
        if (level == llvm::OptimizationLevel::O0) {
            module_pass_manager = pass_builder.buildO0DefaultPipeline(level);
        } else {
            module_pass_manager = pass_builder.buildPerModuleDefaultPipeline(level);
        }

        module_pass_manager.run(*module, module_analysis_manager);
    }

    void initialize(const TargetInfo& target_info,
                    OptimizationLevel::Type optimization_level = OptimizationLevel::Type::O0) {
        [[maybe_unused]] static bool initialized = []() {
            llvm::InitializeAllTargetInfos();
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmParsers();
            llvm::InitializeAllAsmPrinters();
            return true;
        }();

        llvm::Triple target_triple(target_info.triple);
        module->setTargetTriple(target_triple);

        std::string error;
        const auto* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
        if (target == nullptr) {
            throw std::runtime_error("could not find target: " + error);
        }

        llvm::TargetOptions options;
        auto reloc_model = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
        target_machine.reset(target->createTargetMachine(target_triple, "generic", "", options,
                                                         reloc_model, std::nullopt,
                                                         codegen_level(optimization_level)));
        if (target_machine == nullptr) {
            throw std::runtime_error("failed to create target machine");
        }

        module->setDataLayout(target_machine->createDataLayout());
    }

    std::expected<void, std::string> emit_object(const std::filesystem::path& output) {
        std::error_code error_code;
        llvm::raw_fd_ostream output_stream(output.string(), error_code, llvm::sys::fs::OF_None);
        if (error_code) {
            return std::unexpected("could not open output file: " + error_code.message());
        }

        llvm::legacy::PassManager pass;
        if (target_machine->addPassesToEmitFile(pass, output_stream, nullptr,
                                                llvm::CodeGenFileType::ObjectFile)) {
            return std::unexpected("target machine can't emit a file of this type");
        }

        pass.run(*module);
        output_stream.flush();
        return {};
    }
};
