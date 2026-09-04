module;

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

export module zep.build.plan;

import zep.build.artifact;
import zep.codegen.api;
import zep.hir.program;
import zep.common.system.command;

export class CompileAction {
  public:
    std::shared_ptr<const HIRProgram> program;
    std::filesystem::path output;
    CodegenOptions options;

    CompileAction(std::shared_ptr<const HIRProgram> program, std::filesystem::path output,
                  CodegenOptions options)
        : program(std::move(program)), output(std::move(output)), options(std::move(options)) {}
};

export class LinkAction {
  public:
    Command invocation;
    ExecutableArtifact output;

    LinkAction(Command invocation, ExecutableArtifact output)
        : invocation(std::move(invocation)), output(std::move(output)) {}
};

export class BuildPlan {
  public:
    std::vector<CompileAction> compilations;
    std::vector<LinkAction> links;

    void add_compile(std::shared_ptr<const HIRProgram> program, std::filesystem::path output,
                     CodegenOptions options) {
        compilations.emplace_back(std::move(program), std::move(output), std::move(options));
    }

    void add_link(Command invocation, ExecutableArtifact output) {
        links.emplace_back(std::move(invocation), std::move(output));
    }
};
