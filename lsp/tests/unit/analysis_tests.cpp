#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

import zep.common.diagnostic.diagnostic;
import zep.common.source.position;
import zep.lsp.analysis.analyzer;
import zep.lsp.analysis.snapshot;
import zep.lsp.analysis.types;
import zep.test.support;
import zep.common.diagnostic.collection;

using CompletionItem = Completion;

class AnalysisHarness {
  private:
    Analyzer analyzer;

    static std::filesystem::path path(const std::string& uri) {
        return uri.starts_with("file://") ? std::filesystem::path(uri.substr(7))
                                          : std::filesystem::path(uri);
    }

  public:
    AnalysisHarness() = default;

    explicit AnalysisHarness(std::filesystem::path standard_library)
        : analyzer(std::move(standard_library)) {}

    std::vector<Diagnostic> analyze(const std::string& uri, const std::string& content) {
        auto analysis = analyzer.analyze(path(uri), content);
        return analysis.diagnostics();
    }

    std::optional<Hover> hover(const std::string& uri, const std::string& content,
                               Position position) {
        auto analysis = analyzer.analyze(path(uri), content);
        return analysis.hover(Position(position.line + 1, position.column + 1));
    }

    std::vector<Completion> complete(const std::string& uri, const std::string& content,
                                     Position position) {
        analyzer.overlay(path(uri), content);
        return analyzer.complete(path(uri), Position(position.line + 1, position.column + 1));
    }

    std::vector<SemanticToken> semantic_tokens(const std::string& uri, const std::string& content) {
        auto analysis = analyzer.analyze(path(uri), content);
        return analysis.tokens();
    }
};

TEST(LspAnalysis, ProducesDiagnosticsOnSyntaxError) {
    AnalysisHarness service;
    std::string invalid_code = "var = 123;";

    auto diagnostics = service.analyze("file:///test.zep", invalid_code);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, ProducesDiagnosticsOnTypeError) {
    AnalysisHarness service;
    std::string invalid_code = "fn main() -> void { var x: i32 = \"hello\"; }";

    auto diagnostics = service.analyze("file:///test.zep", invalid_code);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, ProducesNoDiagnosticsOnValidCode) {
    AnalysisHarness service;
    std::string valid_code = "fn add(a: i32, b: i32) -> i32 { return a + b; }";

    auto diagnostics = service.analyze("file:///test.zep", valid_code);
    EXPECT_TRUE(diagnostics.empty());
}

TEST(LspAnalysis, ReturnsHoverForVariable) {
    AnalysisHarness service;
    std::string code = "fn test() -> void { var val: i32 = 42; }";

    Position pos(0, 25);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("val"), std::string::npos);
    EXPECT_NE(hover->contents.find("i32"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForFunction) {
    AnalysisHarness service;
    std::string code = "fn compute(x: i32) -> i32 { return x; }";

    Position pos(0, 4);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("compute"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForStruct) {
    AnalysisHarness service;
    std::string code = "struct Point { public: var x: i32 var y: i32 }";

    Position pos(0, 8);
    auto hover = service.hover("file:///test.zep", code, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("Point"), std::string::npos);
}

TEST(LspAnalysis, ReturnsCompletionsIncludingKeywordsPrimitivesAndSymbols) {
    AnalysisHarness service;
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
    AnalysisHarness service;
    std::string code = "fn add(a: i32, b: i32) -> i32 { return a + b; }";

    auto tokens = service.semantic_tokens("file:///test.zep", code);
    EXPECT_FALSE(tokens.empty());
}

TEST(LspAnalysis, ProjectUnderstandingResolvesImports) {
    TestWorkspace workspace("lsp_project_test");
    workspace.write("zep.json", R"({
  "name": "test_proj",
  "version": "0.1.0",
  "type": "executable",
  "target": [{"triple": "x86_64-unknown-linux-gnu"}],
  "libs": {}
})");
    workspace.write("src/helper.zep", "public fn greet() -> i32 { return 42; }");
    std::string main_code = "import helper.greet\npublic fn main() -> i32 { return greet(); }";
    workspace.write("src/main.zep", main_code);

    AnalysisHarness service;
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

TEST(LspAnalysis, LooseFilesResolveStandardLibrarySymbols) {
    AnalysisHarness service(TestPaths::project_source_root() / "std");
    std::string code = "import std.math.Math\nfn main() -> f64 { return Math.absolute(4.0) }";

    auto diagnostics = service.analyze("file:///loose.zep", code);

    EXPECT_TRUE(diagnostics.empty());
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

    AnalysisHarness service;
    auto uri = "file://" + main_file.string();

    auto diagnostics = service.analyze(uri, content);
    EXPECT_TRUE(diagnostics.empty());

    Position hover_position(5, 12);
    auto hover_result = service.hover(uri, content, hover_position);
    ASSERT_TRUE(hover_result.has_value());
    EXPECT_NE(hover_result->contents.find("handle_request"), std::string::npos);

    Position completion_position(0, 0);
    auto completions = service.complete(uri, content, completion_position);
    auto has_completion = [&](const std::string& label) {
        return std::ranges::any_of(completions,
                                   [&](const CompletionItem& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_completion("Config"));
    EXPECT_TRUE(has_completion("handle_request"));

    auto tokens = service.semantic_tokens(uri, content);
    EXPECT_FALSE(tokens.empty());
}

TEST(LspAnalysis, SupportsInMemoryBufferOverrides) {
    TestWorkspace workspace("buffer_override_test");
    workspace.copy_fixture("fixtures/multi_module_project");
    auto project_root = workspace.file("fixtures/multi_module_project");
    auto main_file = project_root / "src/main.zep";

    AnalysisHarness service;
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
    AnalysisHarness service;
    for (const auto& file_name : {"ffi.zep", "pong.zep", "graphics.zep", "main.zep"}) {
        auto path =
            std::filesystem::path("/mnt/code/budchirp/zep-lang/examples/pong/src") / file_name;
        std::ifstream file(path);
        ASSERT_TRUE(file.is_open());
        std::ostringstream ss;
        ss << file.rdbuf();
        auto content = ss.str();
        auto uri = "file://" + path.string();
        auto diags = service.analyze(uri, content);
        auto tokens = service.semantic_tokens(uri, content);
        service.hover(uri, content, Position(5, 5));
        service.complete(uri, content, Position(5, 5));
    }
}

TEST(LspAnalysis, MemberCompletionOnWindow) {
    std::string code =
        "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window "
        "= Window(900, 520, \"Zep Pong\")\n    window.\n    return 0\n}\n";
    AnalysisHarness service;
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
    std::string code =
        "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window "
        "= Window(900, 520, \"Zep Pong\")\n    window.w\n    return 0\n}\n";
    AnalysisHarness service;
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

TEST(LspAnalysis, CompletesGameMembers) {
    auto code = std::string(
        "import graphics.Window\n"
        "import pong.PongGame\n\n"
        "public fn main() -> i32 {\n"
        "    var window = Window(900, 520, \"Zep Pong\")\n"
        "    var mut game = PongGame(window.width, window.height)\n"
        "    game.\n"
        "    return 0\n"
        "}\n");
    AnalysisHarness service;

    auto items = service.complete(
        "file:///mnt/code/budchirp/zep-lang/examples/pong/src/main.zep", code,
        Position(6, 9));

    EXPECT_TRUE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "update";
    }));
    EXPECT_TRUE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "draw";
    }));
}

TEST(LspAnalysis, LocalVariableCompletionInsideFunction) {
    std::string code =
        "import graphics.Window\nimport pong.PongGame\n\npublic fn main() -> i32 {\n    var window "
        "= Window(900, 520, \"Zep Pong\")\n    win\n    return 0\n}\n";
    AnalysisHarness service;
    auto uri = "file:///mnt/code/budchirp/zep-lang/examples/pong/src/main.zep";
    Position pos(5, 7);
    auto items = service.complete(uri, code, pos);
    auto has_label = [&](const std::string& label) {
        return std::ranges::any_of(items, [&](const auto& item) { return item.label == label; });
    };
    EXPECT_TRUE(has_label("window"));
}

TEST(LspAnalysis, CompletesNamedArguments) {
    auto code = std::string(
        "fn move(horizontal: i32, vertical: i32) -> i32 { return horizontal + vertical }\n"
        "fn main() -> i32 { return move(hor) }\n");
    AnalysisHarness service;

    auto items = service.complete("file:///named_arguments.zep", code, Position(1, 33));
    auto iterator = std::ranges::find_if(items, [](const Completion& completion) {
        return completion.label == "horizontal";
    });

    ASSERT_NE(iterator, items.end());
    EXPECT_EQ(iterator->insertion, "horizontal: ");
    EXPECT_FALSE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "vertical";
    }));
}

TEST(LspAnalysis, OmitsAlreadySuppliedNamedArguments) {
    auto code = std::string(
        "fn move(horizontal: i32, vertical: i32) -> i32 { return horizontal + vertical }\n"
        "fn main() -> i32 { return move(horizontal: 1, ver) }\n");
    AnalysisHarness service;

    auto items = service.complete("file:///named_arguments.zep", code, Position(1, 48));

    EXPECT_TRUE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "vertical" && completion.insertion == "vertical: ";
    }));
    EXPECT_FALSE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "horizontal";
    }));
}

TEST(LspAnalysis, CompletesStaticMethods) {
    auto code = std::string(
        "struct Counter { public: var value: i32 }\n"
        "static fn Counter.zero() -> Counter { return Counter { value: 0 } }\n"
        "fn main() -> i32 { return Counter::ze().value }\n");
    AnalysisHarness service;

    auto items = service.complete("file:///static_methods.zep", code, Position(2, 36));

    EXPECT_TRUE(std::ranges::any_of(items, [](const Completion& completion) {
        return completion.label == "zero" && completion.kind == CompletionKind::Type::Method;
    }));
}

TEST(LspAnalysis, CompletesLocalImportModulesAndExports) {
    TestWorkspace workspace("import_completion_test");
    workspace.write("zep.json", R"({
  "name": "imports",
  "version": "0.1.0",
  "type": "executable",
  "target": [{"triple": "x86_64-unknown-linux-gnu"}],
  "libs": {}
})");
    workspace.write("src/helper.zep",
                    "public fn greet() -> i32 { return 1; }\nfn hidden() -> i32 { return 0; }");
    auto main_path = workspace.file("src/main.zep");
    Analyzer analyzer;

    analyzer.overlay(main_path, "import ");
    auto roots = analyzer.complete(main_path, Position(1, 8));
    EXPECT_TRUE(std::ranges::any_of(roots, [](const Completion& completion) {
        return completion.label == "helper";
    }));

    analyzer.overlay(main_path, "import he");
    auto modules = analyzer.complete(main_path, Position(1, 10));
    EXPECT_TRUE(std::ranges::any_of(modules, [](const Completion& completion) {
        return completion.label == "helper" && completion.kind == CompletionKind::Type::Module;
    }));

    analyzer.overlay(main_path, "import helper.");
    auto exports = analyzer.complete(main_path, Position(1, 15));
    EXPECT_TRUE(std::ranges::any_of(exports, [](const Completion& completion) {
        return completion.label == "greet";
    }));
    EXPECT_FALSE(std::ranges::any_of(exports, [](const Completion& completion) {
        return completion.label == "hidden";
    }));

    analyzer.overlay(main_path, "import helper.greet\nimport helper.");
    auto deduplicated = analyzer.complete(main_path, Position(2, 15));
    EXPECT_FALSE(std::ranges::any_of(deduplicated, [](const Completion& completion) {
        return completion.label == "greet";
    }));

    analyzer.overlay(main_path, "import helper.greet as ");
    EXPECT_TRUE(analyzer.complete(main_path, Position(1, 24)).empty());
}

TEST(LspAnalysis, CompletesNestedAndStandardLibraryImports) {
    TestWorkspace workspace("nested_import_completion_test");
    workspace.write("zep.json", R"({
  "name": "imports",
  "version": "0.1.0",
  "type": "executable",
  "target": [{"triple": "x86_64-unknown-linux-gnu"}],
  "libs": {}
})");
    workspace.write("src/net/http/index.zep", "public struct Server {}");
    auto main_path = workspace.file("src/main.zep");
    Analyzer analyzer(TestPaths::project_source_root() / "std");

    analyzer.overlay(main_path, "import net.");
    auto local = analyzer.complete(main_path, Position(1, 12));
    EXPECT_TRUE(std::ranges::any_of(local, [](const Completion& completion) {
        return completion.label == "http";
    }));

    analyzer.overlay(main_path, "import std.net.");
    auto standard = analyzer.complete(main_path, Position(1, 16));
    EXPECT_TRUE(std::ranges::any_of(standard, [](const Completion& completion) {
        return completion.label == "http" || completion.label == "socket";
    }));
}

TEST(LspAnalysis, FindsDefinitionsReferencesHighlightsAndSignatures) {
    auto path = std::filesystem::path("/tmp/compiler_navigation.zep");
    std::string source = "fn value() -> i32 { return 1; }\n"
                         "fn main() -> i32 { return value(); }";
    Analyzer analyzer;
    auto analysis = analyzer.analyze(path, source);
    auto call_column = source.substr(source.find('\n') + 1).find("value") + 1;

    auto definition = analysis.definition(Position(2, call_column));
    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->span.start.line, 1U);

    auto references = analysis.references(Position(2, call_column), true);
    EXPECT_GE(references.size(), 2U);
    EXPECT_GE(analysis.highlights(Position(2, call_column)).size(), 2U);

    auto signature = analysis.signature(Position(2, call_column + 6));
    ASSERT_TRUE(signature.has_value());
    ASSERT_EQ(signature->signatures.size(), 1U);
    EXPECT_NE(signature->signatures.front().label.find("fn"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHierarchicalDocumentSymbols) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze(
        "/tmp/compiler_symbols.zep",
        "struct Point { public: var x: i32 fn length() -> i32 { return 0; } }\n"
        "fn main() -> i32 { return 0; }");

    auto symbols = analysis.document_symbols();
    ASSERT_EQ(symbols.size(), 2U);
    EXPECT_EQ(symbols.front().name, "Point");
    EXPECT_FALSE(symbols.front().children.empty());
}
