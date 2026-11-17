#include "../support/analysis.hpp"

TEST(LspAnalysis, IndexesNamedTestsAndTheirLocals) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/named.test.zep",
        "fn answer() -> i32 { return 42 }\n"
        "test \"private helper\" { var value = answer(); value }\n");

    EXPECT_TRUE(analysis.diagnostics().empty());
    const auto symbols = analysis.document_symbols();

    ASSERT_EQ(symbols.size(), 2U);
    EXPECT_EQ(symbols[1].name, "private helper");
    EXPECT_FALSE(analysis.tokens().empty());
}

TEST(LspAnalysis, ProducesDiagnosticsOnSyntaxError) {
    AnalysisHarness service;
    const auto diagnostics = service.analyze("file:///test.zep", "var = 123;");

    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, ProducesDiagnosticsOnTypeError) {
    AnalysisHarness service;
    const auto diagnostics = service.analyze("file:///test.zep",
        "fn main() -> void { var value: i32 = \"hello\"; }");

    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().severity, DiagnosticSeverity::Type::Error);
}

TEST(LspAnalysis, AnalyzesValidCodeAndProducesSemanticTokens) {
    AnalysisHarness service;
    const auto source = std::string(
        "fn add(left: i32, right: i32) -> i32 { return left + right; }");

    EXPECT_TRUE(service.analyze("file:///test.zep", source).empty());
    EXPECT_FALSE(service.semantic_tokens("file:///test.zep", source).empty());
}

TEST(LspAnalysis, ReturnsHoverForVariable) {
    AnalysisHarness service;
    const auto hover = service.hover("file:///test.zep",
        "fn work() -> void { var val: i32 = 42; }", Position(0, 25));

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("val"), std::string::npos);
    EXPECT_NE(hover->contents.find("i32"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForFunction) {
    AnalysisHarness service;
    const auto hover = service.hover("file:///test.zep",
        "fn compute(value: i32) -> i32 { return value; }", Position(0, 4));

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("compute"), std::string::npos);
}

TEST(LspAnalysis, ReturnsHoverForStruct) {
    AnalysisHarness service;
    const auto hover = service.hover("file:///test.zep",
        "struct Point { public: var x: i32 var y: i32 }", Position(0, 8));

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("Point"), std::string::npos);
}

TEST(LspAnalysis, FindsDefinitionsReferencesHighlightsAndSignatures) {
    const std::string source = "fn value() -> i32 { return 1; }\n"
                               "fn main() -> i32 { return value(); }";
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/compiler_navigation.zep", source);
    const auto column = source.substr(source.find('\n') + 1).find("value") + 1;
    const auto definition = analysis.definition(Position(2, column));

    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->span.start.line, 1U);
    EXPECT_GE(analysis.references(Position(2, column), true).size(), 2U);
    EXPECT_GE(analysis.highlights(Position(2, column)).size(), 2U);

    const auto signature = analysis.signature(Position(2, column + 6));

    ASSERT_TRUE(signature.has_value());
    ASSERT_EQ(signature->signatures.size(), 1U);
    EXPECT_NE(signature->signatures.front().label.find("fn"), std::string::npos);
}

TEST(LspAnalysis, NavigatesToExplicitDestructors) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/compiler_destructor_navigation.zep",
        "struct Owner { public: fn ~Owner() -> void {} }\n"
        "fn clear(ptr: *mut Owner) -> void { ptr->~Owner() }");
    const auto definition = analysis.definition(Position(2, 44));

    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->span.start.line, 1U);
    EXPECT_GE(analysis.references(Position(2, 44), true).size(), 2U);
}

TEST(LspAnalysis, ShowsConcreteGenericCallSignatures) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/compiler_generic_signature.zep",
        "fn identity<T>(value: T) -> T { return value }\n"
        "fn main() -> i32 { return identity(42) }");
    const auto signature = analysis.signature(Position(2, 35));

    ASSERT_TRUE(signature.has_value());
    ASSERT_EQ(signature->signatures.size(), 1U);
    ASSERT_EQ(signature->signatures.front().parameters.size(), 1U);
    EXPECT_EQ(signature->signatures.front().parameters.front().label, "value: i32");
}

TEST(LspAnalysis, ReturnsHierarchicalDocumentSymbols) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/compiler_symbols.zep",
        "struct Point { public: var x: i32 fn length() -> i32 { return 0; } }\n"
        "fn main() -> i32 { return 0; }");
    const auto symbols = analysis.document_symbols();

    ASSERT_EQ(symbols.size(), 2U);
    EXPECT_EQ(symbols.front().name, "Point");
    EXPECT_FALSE(symbols.front().children.empty());
}

TEST(LspAnalysis, ContainsDocumentSelectionsAndMembers) {
    Analyzer analyzer;
    auto analysis = analyzer.analyze("/tmp/outline.zep",
        "struct\n Point {\n public:\n var x: i32\n fn value() -> i32 { return self->x }\n}\n"
        "enum Color { Red Blue }\n"
        "interface Shape { public: fn area() -> i32 }\n"
        "var mut\n count = 1\n");
    const auto symbols = analysis.document_symbols();

    ASSERT_TRUE(analysis.diagnostics().empty());
    ASSERT_EQ(symbols.size(), 4U);
    EXPECT_EQ(symbols[0].selection.start.line, 2U);
    EXPECT_EQ(symbols[0].selection.end.column - symbols[0].selection.start.column, 5U);
    EXPECT_TRUE(symbols[0].location.span.contains(Position(6, 1)));
    EXPECT_EQ(symbols[3].selection.start.line, 10U);
    EXPECT_EQ(symbols[3].selection.end.column - symbols[3].selection.start.column, 5U);
    ASSERT_EQ(symbols[0].children.size(), 2U);
    ASSERT_EQ(symbols[1].children.size(), 2U);
    ASSERT_EQ(symbols[2].children.size(), 1U);

    const auto tokens = analysis.tokens();
    const auto name = std::ranges::find_if(tokens, [](const auto& token) {
        return token.span.start.line == 2 && token.span.start.column == 2;
    });

    ASSERT_NE(name, tokens.end());
    EXPECT_EQ(name->kind, SemanticKind::Type::Struct);

    for (const auto& symbol : symbols) {
        EXPECT_TRUE(symbol.location.span.contains(symbol.selection.start));
        EXPECT_TRUE(symbol.location.span.contains(symbol.selection.end));

        for (const auto& child : symbol.children) {
            EXPECT_TRUE(symbol.location.span.contains(child.location.span.start));
            EXPECT_TRUE(symbol.location.span.contains(child.location.span.end));
            EXPECT_TRUE(child.location.span.contains(child.selection.start));
            EXPECT_TRUE(child.location.span.contains(child.selection.end));
        }
    }
}

TEST(LspAnalysis, NavigatesOwnedCatchBindingsAndCheckedSignatures) {
    Analyzer analyzer(TestPaths::standard_library());
    auto analysis = analyzer.analyze("/tmp/error_navigation.zep",
        "import std.core.error.Error\n"
        "struct Failure : Error { public: var code: i32 }\n"
        "@throws(Failure) fn read() -> i32 { throw Failure { code: 7 } }\n"
        "fn main() -> i32 {\n"
        " return read() catch {\n"
        "  error: Failure -> error.code,\n"
        " }\n"
        "}\n");

    ASSERT_TRUE(analysis.diagnostics().empty());
    const auto hover = analysis.hover(Position(6, 3));

    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("Failure"), std::string::npos);

    const auto definition = analysis.definition(Position(6, 21));

    ASSERT_TRUE(definition.has_value());
    EXPECT_EQ(definition->span.start.line, 6U);
    EXPECT_EQ(definition->span.start.column, 3U);
    EXPECT_EQ(analysis.references(Position(6, 3), true).size(), 2U);
    EXPECT_EQ(analysis.references(Position(6, 3), false).size(), 1U);
    EXPECT_EQ(analysis.highlights(Position(6, 21)).size(), 2U);

    const auto error_type = analysis.definition(Position(6, 11));

    ASSERT_TRUE(error_type.has_value());
    EXPECT_EQ(error_type->span.start.line, 2U);
    EXPECT_EQ(error_type->span.start.column, 8U);

    const auto signature = analysis.signature(Position(5, 14));

    ASSERT_TRUE(signature.has_value());
    EXPECT_NE(signature->signatures.front().label.find("Failure"), std::string::npos);
    EXPECT_EQ(signature->active_parameter, 0U);
}

TEST(LspAnalysis, ShowsCheckedErrorsOnIndirectSignatures) {
    Analyzer analyzer(TestPaths::standard_library());
    auto analysis = analyzer.analyze("/tmp/error_callback.zep",
        "import std.core.error.Error\n"
        "struct Failure : Error {}\n"
        "@throws(Failure) fn read() -> i32 { throw Failure {} }\n"
        "fn main() -> i32 {\n"
        " var reader: @throws(Failure) () -> i32 = read\n"
        " return reader() catch { error: Failure -> 0, }\n"
        "}\n");

    ASSERT_TRUE(analysis.diagnostics().empty());
    const auto signature = analysis.signature(Position(6, 16));

    ASSERT_TRUE(signature.has_value());
    EXPECT_NE(signature->signatures.front().label.find("Failure"), std::string::npos);
    EXPECT_TRUE(signature->signatures.front().parameters.empty());
    EXPECT_EQ(signature->active_parameter, 0U);
}

TEST(LspAnalysis, HighlightsCheckedErrorKeywordsAndReportsUnhandledCalls) {
    Analyzer analyzer(TestPaths::standard_library());
    auto analysis = analyzer.analyze("/tmp/error_tokens.zep",
        "import std.core.error.Error\n"
        "struct Failure : Error {}\n"
        "@throws(Failure) fn fail() -> void { throw Failure {} }\n"
        "fn main() -> void { try { fail() } catch { error: Failure -> {} } }\n");

    ASSERT_TRUE(analysis.diagnostics().empty());

    for (const auto column : {21U, 36U}) {
        const auto tokens = analysis.tokens();
        const auto found = std::ranges::find_if(tokens, [column](const auto& token) {
            return token.span.start.line == 4 && token.span.start.column == column &&
                   token.kind == SemanticKind::Type::Keyword;
        });

        EXPECT_NE(found, tokens.end());
    }

    auto invalid = analyzer.analyze("/tmp/error_tokens.zep",
        "import std.core.error.Error\nstruct Failure : Error {}\n"
        "@throws(Failure) fn fail() -> void { throw Failure {} }\n"
        "fn main() -> void { fail() }\n");

    EXPECT_FALSE(invalid.diagnostics().empty());
}

TEST(LspAnalysis, KeepsNestedCatchBindingsDistinct) {
    Analyzer analyzer(TestPaths::standard_library());
    auto analysis = analyzer.analyze("/tmp/nested_handlers.zep",
        "import std.core.error.Error\n"
        "struct Failure : Error { public: var code: i32 }\n"
        "@throws(Failure) fn read() -> i32 { throw Failure { code: 7 } }\n"
        "fn main() -> i32 {\n"
        " return read() catch { error: Failure -> {\n"
        "  var value = read() catch { error: Failure -> error.code, }\n"
        "  value + error.code\n"
        " }, }\n"
        "}\n");

    ASSERT_TRUE(analysis.diagnostics().empty());
    const auto outer = analysis.definition(Position(7, 11));
    const auto inner = analysis.definition(Position(6, 50));

    ASSERT_TRUE(outer.has_value());
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ(outer->span.start.line, 5U);
    EXPECT_EQ(inner->span.start.line, 6U);
    EXPECT_EQ(analysis.references(Position(7, 11), true).size(), 2U);
    EXPECT_EQ(analysis.references(Position(6, 50), true).size(), 2U);

    for (const auto& token : analysis.tokens()) {
        EXPECT_EQ(token.span.start.line, token.span.end.line);
        EXPECT_LT(token.span.end.column, 100U);
    }
}
