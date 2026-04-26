module;

#include <cstdint>
#include <string>

export module zep.codegen.driver;

import zep.hir.node.program;

export class Backend {
  public:
    enum class Type : std::uint8_t { LLVM };
};

export class CodegenDriver {
  public:
    virtual ~CodegenDriver() = default;
    virtual void generate(HIRProgram& program, const std::string& output_path) = 0;
};
