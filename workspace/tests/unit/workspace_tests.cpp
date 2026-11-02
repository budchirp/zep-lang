#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.common.diagnostic.collection;
import zep.common.target;
import zep.test.support;
import zep.workspace.manifest;
import zep.workspace.project;
import zep.workspace.package.graph;
import zep.common.source;

TEST(WorkspaceManifest, AppliesCurrentDefaults) {
    TestWorkspace workspace("workspace_manifest_defaults");
    workspace.write("zep.json", "{}");

    ManifestReader reader;
    auto manifest = reader.read(workspace.file("zep.json"));

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->name, "workspace_manifest_defaults");
    EXPECT_EQ(manifest->version, "0.1.0");
    EXPECT_EQ(manifest->type, Manifest::Type::Kind::Executable);
    ASSERT_EQ(manifest->targets.size(), 1U);
    EXPECT_EQ(manifest->targets[0].triple, TargetInfo::host_triple());
}

TEST(WorkspaceManifest, ReadsStructuredLinkerArguments) {
    TestWorkspace workspace("workspace_manifest_linker_arguments");
    workspace.write("zep.json", R"({
        "name": "app",
        "target": [{
            "triple": "host",
            "linker": {"arguments": ["-Lvendor", "-lraylib"]}
        }]
    })");

    ManifestReader reader;
    auto manifest = reader.read(workspace.file("zep.json"));

    ASSERT_TRUE(manifest.has_value());
    ASSERT_EQ(manifest->targets.size(), 1U);
    EXPECT_EQ(manifest->targets[0].linker_arguments,
              std::vector<std::string>({"-Lvendor", "-lraylib"}));
}

TEST(WorkspaceManifest, RejectsLegacyLinkerFlags) {
    TestWorkspace workspace("workspace_manifest_legacy_linker_flags");
    workspace.write("zep.json", R"({
        "target": [{"linker": {"flags": "-lm"}}]
    })");

    ManifestReader reader;
    EXPECT_FALSE(reader.read(workspace.file("zep.json")).has_value());
}

TEST(WorkspaceDiagnostics, RetainsSourcesAfterFailedProjectOpen) {
    TestWorkspace workspace("workspace_diagnostic_lifetime");
    workspace.write("zep.json", R"({"name":"app","libs":{"missing_dependency":"0.1.0"}})");

    Diagnostics retained;

    for (auto index = 0; index < 3; ++index) {
        Diagnostics diagnostics;
        EXPECT_FALSE(Project::open(workspace.root(), diagnostics).has_value());
        ASSERT_EQ(diagnostics.entries.size(), 1U);
        retained.entries.push_back(diagnostics.entries.front());
    }

    for (const auto& diagnostic : retained.entries) {
        ASSERT_NE(diagnostic.location.source, nullptr);
        EXPECT_EQ(diagnostic.location.source->name, workspace.file("zep.json").string());
        EXPECT_TRUE(diagnostic.location.source->content.empty());
        EXPECT_NE(diagnostic.message.find("missing_dependency"), std::string::npos);
    }

    testing::internal::CaptureStderr();
    retained.print();
    auto output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("missing_dependency"), std::string::npos);
}

TEST(WorkspaceDiagnostics, RetainsUnreadableDependencyManifest) {
    TestWorkspace workspace("workspace_bad_dependency_manifest");
    workspace.write("dependency/zep.json", "not json");

    Diagnostics diagnostics;
    {
        PackageResolver resolver;
        EXPECT_FALSE(resolver.resolve(workspace.file("dependency"), diagnostics).has_value());
    }

    ASSERT_EQ(diagnostics.entries.size(), 1U);
    EXPECT_EQ(diagnostics.entries.front().location.source->name,
              workspace.file("dependency/zep.json").string());
    EXPECT_EQ(diagnostics.entries.front().message, "could not read manifest");
}
