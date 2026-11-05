module;

#include <memory>
#include <string>
#include <string_view>
#include <utility>

export module zep.compiler.test.harness;

import zep.frontend.test.harness;
import zep.compiler.lowering;
import zep.hir.program;
import zep.hir.node;

export class HirHarness {
  public:
    FrontendHarness frontend;

    explicit HirHarness(std::string content) : frontend(std::move(content)) {
        frontend.type_check();
    }

    std::shared_ptr<const HIRProgram> lower() {
        Lowerer lowerer(frontend.context, frontend.sema);

        return lowerer.lower(frontend.program);
    }

    bool succeeded() const {
        return !frontend.context.diagnostics.has_errors();
    }

    static HIRFunctionDeclaration* function(const HIRProgram& program, std::string_view name) {
        for (auto* statement : program.statements) {
            auto* declaration = statement->as<HIRFunctionDeclaration>();

            if (declaration != nullptr && declaration->name.starts_with(name)) {
                return declaration;
            }
        }

        return nullptr;
    }
};
