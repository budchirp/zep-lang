module;

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

export module zep.build.tests;

import zep.build.objects;
import zep.build.linker;
import zep.compiler;
import zep.compiler.unit;
import zep.compiler.tests;
import zep.compiler.test_sources;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.common.system.command;
import zep.common.system.process;
import zep.common.target;
import zep.codegen.options;
import zep.workspace.project;
import zep.workspace.module_path;
import zep.workspace.toolchain;

export class TestRunner {
  private:
    const Toolchain& toolchain;
    Objects& objects;
    Linker& linker;
    ProcessRunner& process_runner;

    bool execute(const TestSuite& suite, const std::filesystem::path& executable,
                  const std::filesystem::path& directory) {
        auto passed = std::size_t{0};

        for (auto index = std::size_t{0}; index < suite.cases.size(); ++index) {
            const auto& test = suite.cases[index];
            const auto result = process_runner.run(Command(
                {executable.string(), std::to_string(index)}, directory));

            Logger::print(result.succeeded() ? "PASS " : "FAIL ", test.name, "\n");
            Logger::print(result.stdout_text);
            Logger::print_stderr(result.stderr_text);

            if (result.succeeded()) {
                ++passed;
                continue;
            }

            Logger::print_stderr(test.module.source->name, ":", test.declaration.span.start.line,
                ": test failed (",
                result.exit.kind == ProcessExit::Kind::Type::Signaled ? "signal " : "exit ",
                result.exit.status, ")\n");
        }

        Logger::print(passed, " passed; ", suite.cases.size() - passed, " failed\n");

        return passed == suite.cases.size();
    }

  public:
    TestRunner(const Toolchain& toolchain, Objects& objects, Linker& linker,
                ProcessRunner& process_runner)
        : toolchain(toolchain), objects(objects), linker(linker), process_runner(process_runner) {}

    bool run(const std::filesystem::path& path, const std::filesystem::path& input,
              const std::string& filter, bool list, const EmitOptions& options) {
        if (options.target.arch != TargetArch::Kind::Type::Amd64 ||
            options.target.os != TargetOS::Kind::Type::Linux) {
            return Logger::fail("test execution currently requires Linux x86_64");
        }

        Diagnostics diagnostics;
        auto project = Project::open(input.empty() ? path : input, diagnostics, toolchain);

        if (!project.has_value() && !input.empty() && !diagnostics.has_errors()) {
            project = Project::single_file(input, toolchain);
        }

        if (!project.has_value()) {
            diagnostics.print();

            return Logger::fail("could not open test project");
        }

        Compiler compiler(options.target);

        if (!TestSources::load(compiler, *project, input, diagnostics)) {
            diagnostics.print();

            return false;
        }

        TestSuite suite(compiler.sema, compiler.modules, *project->root_package, filter);
        const auto directory = project->root_directory / "build/tests";

        if (!list && !suite.cases.empty()) {
            const auto source = directory / "runner.zep";
            auto name = std::string("__zep_test_runner");

            while (std::ranges::any_of(compiler.modules, [&](const Module* module) {
                return module->owner == project->root_package && module->path.string() == name;
            })) {
                name += '_';
            }

            compiler.overlay(source, suite.dispatcher());
            compiler.load(*project->root_package, ModulePath::from_string(name),
                          diagnostics, source);
        }

        if (diagnostics.has_errors() || !compiler.check(diagnostics)) {
            diagnostics.print();

            return false;
        }

        if (suite.cases.empty()) {
            return Logger::fail(filter.empty() ? "no tests found" : "no tests match the filter");
        }

        if (list) {
            for (const auto& test : suite.cases) {
                Logger::print(test.name, "\n");
            }

            return true;
        }

        const auto outputs = objects.generate_all(compiler, *project->root_package,
            diagnostics, directory / "objects", options);

        if (!outputs.has_value()) {
            return false;
        }

        std::vector<std::string> arguments;

        for (const auto& target : project->root_package->manifest.targets) {
            TargetInfo configured(target.triple);

            if (configured.arch == options.target.arch && configured.os == options.target.os) {
                arguments = target.linker_arguments;
                break;
            }
        }

        const auto executable = directory / "runner";

        if (!linker.link(*outputs, executable, options.target, arguments, project->root_directory)) {
            return false;
        }

        return execute(suite, executable, project->root_directory);
    }
};
