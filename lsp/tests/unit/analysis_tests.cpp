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
