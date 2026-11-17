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

export void write_runtime_fixture(CliTestHarness& harness, const std::filesystem::path& source) {
    write_project_config(harness, source.stem().stem().string());
    harness.workspace.write("src/main.zep", TextFile::read(source));
}

export void write_test_group(CliTestHarness& harness, const std::filesystem::path& directory) {
    write_project_config(harness, directory.filename().string());

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename().string().ends_with(".test.zep")) {
            harness.workspace.write(std::filesystem::path("tests") / entry.path().filename(),
                                    TextFile::read(entry.path()));
        }
    }
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
