module;

#include <cstdint>
#include <optional>
#include <utility>

export module zep.codegen.options;

import zep.common.target;

export class OptimizationLevel {
  public:
    enum class Type : std::uint8_t { O0, O1, O2, O3 };

    static std::optional<Type> from_int(int value) {
        if (value < 0 || value > 3) {
            return std::nullopt;
        }
        return static_cast<Type>(value);
    }

    static int to_int(Type level) { return static_cast<int>(level); }
};

export class DebugOutput {
  public:
    enum class Type : std::uint8_t { None, IR };
};

export class CodegenOptions {
  public:
    TargetInfo target;
    OptimizationLevel::Type optimization;
    DebugOutput::Type debug_output;

    explicit CodegenOptions(TargetInfo target = TargetInfo(),
                            OptimizationLevel::Type optimization = OptimizationLevel::Type::O0,
                            DebugOutput::Type debug_output = DebugOutput::Type::None)
        : target(std::move(target)), optimization(optimization), debug_output(debug_output) {}
};
