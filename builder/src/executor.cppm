module;

#include <string>
#include <utility>
#include <vector>

export module zep.build.executor;

import zep.build.artifact;
import zep.build.plan;
import zep.common.system.process;
import zep.codegen.api;

export class BuildFailure {
  public:
    std::string message;

    explicit BuildFailure(std::string message) : message(std::move(message)) {}
};

export class BuildResult {
  public:
    std::vector<ObjectArtifact> objects;
    std::vector<ExecutableArtifact> executables;
    std::vector<BuildFailure> failures;

    bool succeeded() const { return failures.empty(); }
};

export class BuildExecutor {
  public:
    static BuildResult execute(const BuildPlan& plan, CodegenBackend& backend,
                               ProcessRunner& process_runner) {
        BuildResult result;
        result.objects.reserve(plan.compilations.size());
        result.executables.reserve(plan.links.size());

        for (const auto& action : plan.compilations) {
            auto codegen = backend.generate(*action.program, action.output, action.options);
            if (!codegen.has_value()) {
                result.failures.emplace_back(codegen.error());
                return result;
            }
            result.objects.emplace_back(action.output);
        }

        for (const auto& action : plan.links) {
            auto process = process_runner.run(action.invocation);
            if (!process.succeeded()) {
                result.failures.emplace_back(process.stderr_text);
                return result;
            }

            result.executables.push_back(action.output);
        }

        return result;
    }
};
