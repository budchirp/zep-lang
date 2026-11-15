module;

#include <filesystem>
#include <optional>
#include <vector>

export module zep.build.objects;

import zep.codegen.api;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.compiler;
import zep.compiler.unit;
import zep.workspace.module_path;
import zep.workspace.package;

export class Objects {
  private:
    Backend& backend;

  public:
    explicit Objects(Backend& backend) : backend(backend) {}

    std::optional<std::vector<std::filesystem::path>> generate_all(
        Compiler& compiler, const Package& root, Diagnostics& diagnostics,
        const std::filesystem::path& directory, const EmitOptions& options) {
        std::vector<std::filesystem::path> outputs;
        outputs.reserve(compiler.modules.size());

        for (auto* module : compiler.modules) {
            auto relative = ModulePath::is_test(module->source->name)
                ? std::filesystem::path(module->source->name).lexically_relative(module->owner->root)
                : module->path.path();
            relative.replace_extension(".o");
            const auto output = module->owner == &root
                ? directory / relative
                : directory.parent_path() / "dependencies" /
                  module->owner->manifest.name / relative;

            if (!generate(compiler, *module, diagnostics, output, options)) {
                return std::nullopt;
            }

            outputs.push_back(output);
        }

        return outputs;
    }

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
