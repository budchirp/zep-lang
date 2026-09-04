#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.common.diagnostic.collection;
import zep.test.support;
import zep.workspace.manifest;
import zep.workspace.package.graph;
import zep.workspace.package;

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
    EXPECT_TRUE(manifest->targets[0].default_layout);
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

TEST(WorkspacePackages, TraversesDependenciesDeterministically) {
    PackageGraph graph;
    auto& root = graph.add(Manifest("root", "0.1.0", Manifest::Type::Kind::Executable, {}, {}),
                           PackageSource::Type::Workspace, ".");
    auto& alpha = graph.add(Manifest("alpha", "0.1.0", Manifest::Type::Kind::Library, {}, {}),
                            PackageSource::Type::Path, "alpha");
    auto& beta = graph.add(Manifest("beta", "0.1.0", Manifest::Type::Kind::Library, {}, {}),
                           PackageSource::Type::Path, "beta");
    root.dependencies.push_back(&beta);
    root.dependencies.push_back(&alpha);

    auto packages = graph.traversal({"root"});

    ASSERT_EQ(packages.size(), 3U);
    EXPECT_EQ(packages[0]->manifest.name, "alpha");
    EXPECT_EQ(packages[1]->manifest.name, "beta");
    EXPECT_EQ(packages[2]->manifest.name, "root");
}
