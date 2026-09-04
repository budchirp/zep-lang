#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.compiler;
import zep.compiler.graph;
import zep.frontend.sema.context;
import zep.compiler.resolver;
import zep.common.diagnostic.collection;
import zep.test.support;
import zep.workspace.manifest;
import zep.workspace.module_path;
import zep.workspace.package;
import zep.workspace.package.graph;
import zep.workspace.package;

namespace {

Package& add_package(PackageGraph& graph, const std::string& name,
                     const std::filesystem::path& root) {
    return graph.add(Manifest(name, "0.1.0", Manifest::Type::Kind::Library, {}, {}),
                     PackageSource::Type::Path, root);
}

} // namespace

TEST(CompilerResolver, PrefersLocalFileToIndex) {
    TestWorkspace workspace("compiler_resolver_file_index");
    workspace.write("src/main.zep", "");
    workspace.write("src/main/index.zep", "");

    PackageGraph packages;
    auto& package = add_package(packages, "app", workspace.root());
    ModuleResolver resolver;

    auto resolved = resolver.resolve(package, ModulePath::from_string("main"));

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->source_path, workspace.file("src/main.zep"));
}

TEST(CompilerResolver, ResolvesDependencyRootLib) {
    TestWorkspace workspace("compiler_resolver_package_root");
    workspace.write("app/src/main.zep", "");
    workspace.write("dep/src/lib.zep", "");

    PackageGraph packages;
    auto& app = add_package(packages, "app", workspace.file("app"));
    auto& dependency = add_package(packages, "dep", workspace.file("dep"));
    app.dependencies.push_back(&dependency);
    ModuleResolver resolver;

    auto resolved = resolver.resolve(app, ModulePath::from_string("dep"));

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->owner, &dependency);
    EXPECT_EQ(resolved->path.string(), "lib");
    EXPECT_EQ(resolved->source_path, workspace.file("dep/src/lib.zep"));
}

TEST(CompilerResolver, PrefersLocalShadowingDependency) {
    TestWorkspace workspace("compiler_resolver_shadowing");
    workspace.write("app/src/dep.zep", "");
    workspace.write("dep/src/lib.zep", "");

    PackageGraph packages;
    auto& app = add_package(packages, "app", workspace.file("app"));
    auto& dependency = add_package(packages, "dep", workspace.file("dep"));
    app.dependencies.push_back(&dependency);
    ModuleResolver resolver;

    auto resolved = resolver.resolve(app, ModulePath::from_string("dep"));

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->owner, &app);
    EXPECT_EQ(resolved->source_path, workspace.file("app/src/dep.zep"));
}

TEST(CompilerGraph, OrdersDependenciesBeforeImporters) {
    TestWorkspace workspace("compiler_graph_ordering");
    workspace.write("src/main.zep", "import beta.value;");
    workspace.write("src/beta.zep", "import alpha.value;\npublic fn value() -> i32 { return 1 }");
    workspace.write("src/alpha.zep", "public fn value() -> i32 { return 1 }");

    PackageGraph packages;
    auto& app = add_package(packages, "app", workspace.root());
    Diagnostics diagnostics;
    SemaContext sema;
    ModuleGraph graph(sema);

    ASSERT_NE(graph.load(app, ModulePath::from_string("main"), diagnostics), nullptr);
    ASSERT_FALSE(diagnostics.has_errors());
    ASSERT_EQ(graph.ordering().size(), 3U);
    EXPECT_EQ(graph.ordering()[0]->path.string(), "alpha");
    EXPECT_EQ(graph.ordering()[1]->path.string(), "beta");
    EXPECT_EQ(graph.ordering()[2]->path.string(), "main");
}

TEST(CompilerGraph, ReportsCompleteCycleEdgeChain) {
    TestWorkspace workspace("compiler_graph_cycle");
    workspace.write("src/a.zep", "import b.value;\npublic fn value() -> i32 { return 1 }");
    workspace.write("src/b.zep", "import c.value;\npublic fn value() -> i32 { return 1 }");
    workspace.write("src/c.zep", "import a.value;\npublic fn value() -> i32 { return 1 }");

    PackageGraph packages;
    auto& app = add_package(packages, "app", workspace.root());
    Diagnostics diagnostics;
    SemaContext sema;
    ModuleGraph graph(sema);

    ASSERT_NE(graph.load(app, ModulePath::from_string("a"), diagnostics), nullptr);
    ASSERT_TRUE(diagnostics.has_errors());
    ASSERT_EQ(diagnostics.all().size(), 1U);
    EXPECT_NE(diagnostics.all()[0].message.find("a -> b -> c -> a"), std::string::npos);
    EXPECT_EQ(diagnostics.all()[0].location.source->name, workspace.file("src/c.zep").string());
}

TEST(CompilerSemantics, BindsImportedTypeScopesAndInterfaces) {
    TestWorkspace workspace("compiler_imported_type_bindings");
    workspace.write("src/marker.zep", "public interface Copy {}\n");
    workspace.write("src/library.zep", "import marker.Copy\n"
                                       "public struct Value : Copy {\n"
                                       "    public:\n"
                                       "        fn Value() -> Value { return Value {} }\n"
                                       "        fn value() -> i32 { return 7 }\n"
                                       "}\n");
    workspace.write("src/main.zep", "import library.Value\n"
                                    "public fn main() -> i32 {\n"
                                    "    var value = Value()\n"
                                    "    return value.value()\n"
                                    "}\n");

    PackageGraph packages;
    auto& package = add_package(packages, "app", workspace.root());
    Diagnostics diagnostics;
    Compiler compiler;

    ASSERT_NE(compiler.load(package, ModulePath::from_string("main"), diagnostics), nullptr);
    ASSERT_FALSE(diagnostics.has_errors());
    EXPECT_TRUE(compiler.check());
}
