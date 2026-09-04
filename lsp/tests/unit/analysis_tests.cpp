#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

import zep.lsp.analysis;
import zep.lsp.protocol.types;
import zep.test.support;
import zep.common.diagnostic.collection;
import zep.workspace.toolchain;
import zep.workspace.package.graph;

using namespace lsp;

TEST(LspAnalysis, ProducesDiagnosticsOnSyntaxError) {
    AnalysisService service;
    std::string invalid_code = "var = 123;";

    auto diagnostics = service.analyze("file:///test.zep", invalid_code);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, ProducesDiagnosticsOnTypeError) {
    AnalysisService service;
    std::string invalid_code = "fn main() -> void { var x: i32 = \"hello\"; }";

    auto diagnostics = service.analyze("file:///test.zep", invalid_code);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, ProducesNoDiagnosticsOnValidCode) {
    AnalysisService service;
    std::string valid_code = "fn add(a: i32, b: i32) -> i32 { return a + b; }";

    auto diagnostics = service.analyze("file:///test.zep", valid_code);
    EXPECT_TRUE(diagnostics.empty());
}

TEST(LspAnalysis, ReturnsHoverForVariable) {
    AnalysisService service;
    std::string code = "fn test() -> void { var val: i32 = 42; }";

    Position pos(0, 25);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("val"), std::string::npos);
    EXPECT_NE(hover->contents.find("i32"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForFunction) {
    AnalysisService service;
    std::string code = "fn compute(x: i32) -> i32 { return x; }";

    Position pos(0, 4);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("compute"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForStruct) {
    AnalysisService service;
    std::string code = "struct Point { public: var x: i32 var y: i32 }";

    Position pos(0, 8);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("Point"), std::string::npos);
}

TEST(LspAnalysis, ReturnsCompletionsIncludingKeywordsPrimitivesAndSymbols) {
    AnalysisService service;
    std::string code =
        "struct MyStruct { public: var a: i32 }\nfn my_func() -> void {}\nvar my_var: i32 = 0;";

    Position pos(0, 0);
    auto items = service.complete("file:///test.zep", code, pos);

    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items,
                                   [&](const CompletionItem& item) { return item.label == label; });
    };

    EXPECT_TRUE(has_label("fn"));
    EXPECT_TRUE(has_label("var"));
    EXPECT_TRUE(has_label("struct"));
    EXPECT_TRUE(has_label("i32"));
    EXPECT_TRUE(has_label("cstr"));
    EXPECT_TRUE(has_label("#sizeof"));
    EXPECT_TRUE(has_label("MyStruct"));
    EXPECT_TRUE(has_label("my_func"));
    EXPECT_TRUE(has_label("my_var"));
}

TEST(LspAnalysis, ExtractsSemanticTokens) {
    AnalysisService service;
    std::string code = "fn add(a: i32, b: i32) -> i32 { return a + b; }";

    auto tokens = service.semantic_tokens("file:///test.zep", code);
    EXPECT_FALSE(tokens.data.empty());
    EXPECT_EQ(tokens.data.size() % 5, 0U);
}

TEST(LspAnalysis, ProjectUnderstandingResolvesImports) {
    TestWorkspace workspace("lsp_project_test");
    workspace.write("zep.json", R"({
  "name": "test_proj",
  "version": "0.1.0",
  "type": "executable",
  "targets": [{"triple": "x86_64-unknown-linux-gnu"}],
  "libs": {}
})");
    workspace.write("src/helper.zep", "public fn greet() -> i32 { return 42; }");
    std::string main_code = "import helper.greet\npublic fn main() -> i32 { return greet(); }";
    workspace.write("src/main.zep", main_code);

    AnalysisService service;
    auto main_path = workspace.file("src/main.zep");
    auto uri = "file://" + main_path.string();

    auto diagnostics = service.analyze(uri, main_code);
    EXPECT_TRUE(diagnostics.empty());

    Position pos(0, 0);
    auto items = service.complete(uri, main_code, pos);

    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items,
                                   [&](const CompletionItem& item) { return item.label == label; });
    };

    EXPECT_TRUE(has_label("greet"));
}

TEST(LspAnalysis, MultiModuleProjectAnalysis) {
    TestWorkspace workspace("multi_module_test");
    workspace.copy_fixture("fixtures/multi_module_project");
    auto project_root = workspace.file("fixtures/multi_module_project");
    auto main_file = project_root / "src/main.zep";

    std::ifstream file(main_file);
    ASSERT_TRUE(file.is_open());
    std::ostringstream ss;
    ss << file.rdbuf();
    auto content = ss.str();

    AnalysisService service;
    auto uri = "file://" + main_file.string();

    auto diagnostics = service.analyze(uri, content);
    EXPECT_TRUE(diagnostics.empty());

    Position hover_position(5, 12);
    auto hover_result = service.hover(uri, content, hover_position);
    ASSERT_TRUE(hover_result.has_value());
    EXPECT_NE(hover_result->contents.find("handle_request"), std::string::npos);

    Position completion_position(5, 12);
    auto completions = service.complete(uri, content, completion_position);
    auto has_completion = [&](const std::string& label) {
        return std::ranges::any_of(completions,
                                   [&](const CompletionItem& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_completion("Config"));
    EXPECT_TRUE(has_completion("handle_request"));

    auto tokens = service.semantic_tokens(uri, content);
    EXPECT_FALSE(tokens.data.empty());
    EXPECT_EQ(tokens.data.size() % 5, 0U);
}

TEST(LspAnalysis, SupportsInMemoryBufferOverrides) {
    TestWorkspace workspace("buffer_override_test");
    workspace.copy_fixture("fixtures/multi_module_project");
    auto project_root = workspace.file("fixtures/multi_module_project");
    auto main_file = project_root / "src/main.zep";

    AnalysisService service;
    auto uri = "file://" + main_file.string();

    std::string bad_code =
        "import handlers.Config\nimport handlers.handle_request\n\npublic fn main() -> i32 {\n    "
        "var x: Config = 12345\n    return 0\n}\n";
    auto diagnostics = service.analyze(uri, bad_code);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);

    std::ifstream disk_file(main_file);
    std::ostringstream ss;
    ss << disk_file.rdbuf();
    EXPECT_NE(ss.str(), bad_code);
}

TEST(LspAnalysis, TestPongFiles) {
    AnalysisService service;
    for (const auto& file_name : {"ffi.zep", "pong.zep", "graphics.zep", "main.zep"}) {
        auto path = std::filesystem::path("/mnt/code/budchirp/zep-lang/examples/pong/src") / file_name;
        std::ifstream file(path);
        ASSERT_TRUE(file.is_open());
        std::ostringstream ss;
        ss << file.rdbuf();
        auto content = ss.str();
        auto uri = "file://" + path.string();
        std::cout << "Testing analyze: " << file_name << std::endl;
        auto diags = service.analyze(uri, content);
        std::cout << "Testing tokens: " << file_name << std::endl;
        auto tokens = service.semantic_tokens(uri, content);
        std::cout << "Testing hover: " << file_name << std::endl;
        service.hover(uri, content, Position(5, 5));
        std::cout << "Testing complete: " << file_name << std::endl;
        service.complete(uri, content, Position(5, 5));
    }
}

TEST(LspAnalysis, MemberCompletionOnWindow) {
    std::string code = "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window = Window(900, 520, \"Zep Pong\")\n    window.\n    return 0\n}\n";
    AnalysisService service;
    auto uri = "file:///mnt/code/budchirp/zep-lang/examples/pong/src/main.zep";
    Position pos(5, 11);
    auto items = service.complete(uri, code, pos);
    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items, [&](const auto& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_label("width"));
    EXPECT_TRUE(has_label("height"));
    EXPECT_TRUE(has_label("should_close"));
}

TEST(LspAnalysis, MemberCompletionWithPrefixFilters) {
    std::string code = "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window = Window(900, 520, \"Zep Pong\")\n    window.w\n    return 0\n}\n";
    AnalysisService service;
    auto uri = "file:///mnt/code/budchirp/zep-lang/examples/pong/src/main.zep";
    Position pos(5, 12);
    auto items = service.complete(uri, code, pos);
    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items, [&](const auto& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_label("width"));
    EXPECT_FALSE(has_label("height"));
    EXPECT_FALSE(has_label("should_close"));
}

TEST(LspAnalysis, LocalVariableCompletionInsideFunction) {
    std::string code = "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window = Window(900, 520, \"Zep Pong\")\n    win\n    return 0\n}\n";
    AnalysisService service;
    auto uri = "file:///mnt/code/budchirp/zep-lang/examples/pong/src/main.zep";
    Position pos(5, 7);
    auto items = service.complete(uri, code, pos);
    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items, [&](const auto& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_label("window"));
}
