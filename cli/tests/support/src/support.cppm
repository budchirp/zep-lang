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

export ProcessResult build_and_run(CliTestHarness& harness, const std::string& name) {
    auto build = harness.run({"build"}, harness.workspace.root());
    harness.assert_success_and_exists(std::filesystem::path("build") / name, build);

    if (!build.succeeded()) {
        return build;
    }

    auto executable = harness.workspace.file(std::filesystem::path("build") / name);
    return TestProcessRunner::run({executable.string()}, harness.workspace.root());
}
