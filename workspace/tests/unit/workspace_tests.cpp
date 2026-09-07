#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.common.diagnostic.collection;
import zep.common.target;
import zep.test.support;
import zep.workspace.manifest;

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
