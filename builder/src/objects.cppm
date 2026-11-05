module;

#include <filesystem>

export module zep.build.objects;

import zep.codegen.api;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.compiler;
import zep.compiler.unit;

export class Objects {
  private:
    Backend& backend;

  public:
    explicit Objects(Backend& backend) : backend(backend) {}

    bool generate(Compiler& compiler, Module& module, Diagnostics& diagnostics,
                  const std::filesystem::path& output, const EmitOptions& options) {
        auto program = compiler.lower(module, diagnostics);

        if (program == nullptr || diagnostics.has_errors()) {
            diagnostics.print();

            return false;
        }

        std::filesystem::create_directories(output.parent_path());

        const auto result = backend.generate(*program, output, options);

        if (!result.has_value()) {
            Logger::print_stderr("zep: error: ", result.error(), "\n");

            return false;
        }

        return true;
    }
};
