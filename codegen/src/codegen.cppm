module;

#include <memory>

export module zep.codegen;

import zep.hir.node.program;
import zep.codegen.driver;
import zep.codegen.llvm;

export class Codegen {
    std::unique_ptr<CodegenDriver> driver;

  public:
    explicit Codegen(Backend::Type backend) {
        switch (backend) {
        case Backend::Type::LLVM:
            driver = std::make_unique<LLVMCodegen>();
            break;
        }
    }

    void generate(HIRProgram& program) { driver->generate(program); }
};
