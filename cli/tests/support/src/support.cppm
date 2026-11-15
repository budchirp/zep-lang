module;

#include <filesystem>
#include <string>

export module zep.cli.test.support;

import zep.test.support;

export void write_project_config(CliTestHarness& harness, const std::string& name) {
    harness.workspace.write("zep.json", "{\n"
                                        "    \"name\": \"" +
                                            name +
                                            "\",\n"
                                            "    \"version\": \"0.1.0\",\n"
                                            "    \"libs\": {\n"
                                            "        \"std\": \"0.0.1\"\n"
                                            "    }\n"
                                            "}\n");
}

export void write_runtime_fixture(CliTestHarness& harness, const std::string& name,
                                  bool standard = false, bool named = false) {
    write_project_config(harness, name);

    const auto directory = standard ? TestPaths::project_source_root() / "std/tests"
                                   : TestPaths::source_root() / "fixtures";
    const auto source = directory / (standard && !named ? "fixtures" : "") /
                        (name + ".test.zep");

    harness.workspace.write(named ? "src/main.test.zep" : "src/main.zep", TextFile::read(source));
}

export ProcessResult build_and_run(CliTestHarness& harness, const std::string& name,
                                   const std::string& level = "0") {
    auto build = harness.run({"build", "-O", level}, harness.workspace.root());
    harness.assert_success_and_exists(std::filesystem::path("build") / name, build);

    if (!build.succeeded()) {
        return build;
    }

    auto executable = harness.workspace.file(std::filesystem::path("build") / name);
    return TestProcessRunner::run({executable.string()}, harness.workspace.root());
}
