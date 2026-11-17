module;

#include <optional>
#include <string>
#include <vector>

export module zep.cli.commands.codegen;

import argman;
import zep.codegen.options;
import zep.common.logger;
import zep.common.target;

export class CodegenArguments {
  public:
    static std::vector<argman::Option> options(std::vector<argman::Option> options) {
        options.reserve(options.size() + 2);
        options.emplace_back("emit-ir", "Print LLVM IR after code generation", false, true);
        options.emplace_back("optimization", "Optimization level (0, 1, 2, or 3)", 0, false,
                             std::vector<std::string>{"O"});

        return options;
    }

    static std::optional<EmitOptions> parse(int level, bool emit_ir, const std::string& triple = "") {
        const auto optimization = OptimizationLevel::from_int(level);

        if (!optimization.has_value()) {
            Logger::print_stderr("zep: error: optimization level must be 0, 1, 2, or 3\n");
            return std::nullopt;
        }

        return EmitOptions(TargetInfo(triple.empty() ? TargetInfo::host_triple() : triple),
            *optimization, emit_ir ? DebugOutput::Type::IR : DebugOutput::Type::None);
    }
};
