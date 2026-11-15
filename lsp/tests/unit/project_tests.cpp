#include "../support/analysis.hpp"

TEST(LspAnalysis, ResolvesPrivateSiblingsFromMirroredTestFiles) {
    TestWorkspace workspace("private_test_completion");
    workspace.write("zep.json", R"({"name":"private_tests","type":"library"})");
    workspace.write("src/math.zep", "fn secret() -> i32 { return 42 }\n");
    workspace.write("tests/math.test.zep", "test \"private sibling\" { var value = secret() }\n");

    Analyzer analyzer;
    const auto path = workspace.file("tests/math.test.zep");
    const auto analysis = analyzer.analyze(path);

    EXPECT_TRUE(analysis.diagnostics().empty());

    const auto definition = analysis.definition(Position(1, 38));

    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->path, workspace.file("src/math.zep"));

    analyzer.overlay(path, "test \"private sibling\" { sec }");
    const auto completions = analyzer.complete(path, Position(1, 29));

    EXPECT_NE(find_completion(completions, "secret"), nullptr);
}

TEST(LspAnalysis, CompletesLocalImportModulesAndExports) {
    TestWorkspace workspace("import_completion_test");
    workspace.write("zep.json", R"({"name":"imports","version":"0.1.0","libs":{}})");
    workspace.write("src/helper.zep",
        "public fn greet() -> i32 { return 1; }\nfn hidden() -> i32 { return 0; }");
    const auto path = workspace.file("src/main.zep");
    Analyzer analyzer;

    analyzer.overlay(path, "import ");
    const auto roots = analyzer.complete(path, Position(1, 8));

    EXPECT_NE(find_completion(roots, "helper"), nullptr);

    analyzer.overlay(path, "import he");
    const auto modules = analyzer.complete(path, Position(1, 10));
    const auto* module = find_completion(modules, "helper");

    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->kind, CompletionKind::Type::Module);

    analyzer.overlay(path, "import helper.");
    const auto exports = analyzer.complete(path, Position(1, 15));

    EXPECT_NE(find_completion(exports, "greet"), nullptr);
    EXPECT_EQ(find_completion(exports, "hidden"), nullptr);

    analyzer.overlay(path, "import helper.greet\nimport helper.");
    const auto repeated = analyzer.complete(path, Position(2, 15));

    EXPECT_EQ(find_completion(repeated, "greet"), nullptr);

    analyzer.overlay(path, "import helper.greet as ");

    EXPECT_TRUE(analyzer.complete(path, Position(1, 24)).empty());
}

TEST(LspAnalysis, CompletesNestedAndStandardLibraryImports) {
    TestWorkspace workspace("nested_import_completion_test");
    workspace.write("zep.json", R"({"name":"imports","version":"0.1.0","libs":{}})");
    workspace.write("src/net/http/index.zep", "public struct Server {}");
    const auto path = workspace.file("src/main.zep");
    Analyzer analyzer(TestPaths::project_source_root() / "std");

    analyzer.overlay(path, "import net.");
    const auto local = analyzer.complete(path, Position(1, 12));

    EXPECT_NE(find_completion(local, "http"), nullptr);

    analyzer.overlay(path, "import std.net.");
    const auto standard = analyzer.complete(path, Position(1, 16));

    EXPECT_NE(find_completion(standard, "http"), nullptr);
    EXPECT_NE(find_completion(standard, "socket"), nullptr);
}

TEST(LspAnalysis, LooseFilesResolveStandardLibrarySymbols) {
    AnalysisHarness service(TestPaths::project_source_root() / "std");
    const auto diagnostics = service.analyze("file:///loose.zep",
        "import std.math.Math\nfn main() -> f64 { return Math.absolute(4.0) }");

    EXPECT_TRUE(diagnostics.empty());
}

TEST(LspAnalysis, MultiModuleProjectAnalysis) {
    TestWorkspace workspace("multi_module_test");
    workspace.copy_fixture("fixtures/multi_module_project");
    const auto path = workspace.file("fixtures/multi_module_project/src/main.zep");
    const auto content = TextFile::read(path);
    const auto uri = "file://" + path.string();
    AnalysisHarness service;

    EXPECT_TRUE(service.analyze(uri, content).empty());

    const auto hover = service.hover(uri, content, Position(5, 12));

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("handle_request"), std::string::npos);

    const auto completions = service.complete(uri, content, Position(0, 0));

    EXPECT_NE(find_completion(completions, "Config"), nullptr);
    EXPECT_NE(find_completion(completions, "handle_request"), nullptr);
    EXPECT_FALSE(service.semantic_tokens(uri, content).empty());
}

TEST(LspAnalysis, SupportsInMemoryBufferOverrides) {
    TestWorkspace workspace("buffer_override_test");
    workspace.copy_fixture("fixtures/multi_module_project");
    const auto path = workspace.file("fixtures/multi_module_project/src/main.zep");
    const auto original = TextFile::read(path);
    AnalysisHarness service;
    const auto diagnostics = service.analyze("file://" + path.string(),
        "import handlers.Config\nimport handlers.handle_request\n\n"
        "public fn main() -> i32 { var value: Config = 12345; return 0; }");

    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
    EXPECT_EQ(TextFile::read(path), original);
}

TEST(LspAnalysis, AnalyzesPongSources) {
    AnalysisHarness service(TestPaths::project_source_root() / "std");

    for (const auto* name : {"ffi.zep", "pong.zep", "graphics.zep", "main.zep"}) {
        SCOPED_TRACE(name);
        const auto path = TestPaths::project_source_root() / "examples/pong/src" / name;
        const auto content = TextFile::read(path);
        const auto uri = "file://" + path.string();

        EXPECT_TRUE(service.analyze(uri, content).empty());
        EXPECT_FALSE(service.semantic_tokens(uri, content).empty());
    }
}

TEST(LspAnalysis, MemberCompletionOnWindow) {
    AnalysisHarness service(TestPaths::project_source_root() / "std");
    const auto uri = "file://" +
        (TestPaths::project_source_root() / "examples/pong/src/main.zep").string();
    const auto prefix = std::string(
        "import graphics.Window\nfn main() -> void { var window = Window(900, 520, \"Pong\"); window.");
    const auto column = prefix.size() - prefix.find('\n') - 1;
    const auto members = service.complete(uri, prefix + " }", Position(1, column));

    EXPECT_NE(find_completion(members, "width"), nullptr);
    EXPECT_NE(find_completion(members, "height"), nullptr);
    EXPECT_NE(find_completion(members, "should_close"), nullptr);

    const auto filtered = service.complete(uri, prefix + "w }", Position(1, column + 1));

    EXPECT_NE(find_completion(filtered, "width"), nullptr);
    EXPECT_EQ(find_completion(filtered, "height"), nullptr);
    EXPECT_EQ(find_completion(filtered, "should_close"), nullptr);
}

TEST(LspAnalysis, CompletesGameMembers) {
    AnalysisHarness service(TestPaths::project_source_root() / "std");
    const auto uri = "file://" +
        (TestPaths::project_source_root() / "examples/pong/src/main.zep").string();
    const auto content = std::string(
        "import graphics.Window\n"
        "import pong.PongGame\n\n"
        "public fn main() -> i32 {\n"
        "    var window = Window(900, 520, \"Zep Pong\")\n"
        "    var mut game = PongGame(window.width as f32, window.height as f32)\n"
        "    game.\n"
        "    return 0\n"
        "}\n");
    const auto items = service.complete(uri, content, Position(6, 9));

    EXPECT_NE(find_completion(items, "update"), nullptr);
    EXPECT_NE(find_completion(items, "draw"), nullptr);
}
