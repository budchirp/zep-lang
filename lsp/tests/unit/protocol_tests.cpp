#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

import zep.common.source.position;
import zep.common.source.span;
import zep.lsp.analysis.types;
import zep.lsp.document;
import zep.lsp.protocol;
import zep.lsp.session;
import zep.lsp.transport;

class LspProtocolTest : public testing::Test {
  protected:
    std::istringstream input;
    std::ostringstream output;
    Transport transport{input, output};
    Session session{transport, std::filesystem::path()};
    ProtocolCodec protocol{session};
};

TEST_F(LspProtocolTest, ConvertsRangesToUtf16Positions) {
    Document document("file:///test.zep", 1, "a😀b\nsecond");
    auto range = protocol.range(document, Span(Position(1, 2), Position(1, 6)));

    EXPECT_EQ(range["start"]["line"], 0);
    EXPECT_EQ(range["start"]["character"], 1);
    EXPECT_EQ(range["end"]["character"], 3);
}

TEST_F(LspProtocolTest, ContainsSymbolSelectionsAndUsesSymbolKinds) {
    Document document("file:///test.zep", 1, "struct Point {\n var x: i32\n}\n");
    DocumentSymbol field("x", CompletionKind::Type::Field,
        AnalysisLocation("/test.zep", Span(Position(2, 2), Position(2, 5))),
        Span(Position(2, 6), Position(2, 7)));
    DocumentSymbol structure("Point", CompletionKind::Type::Struct,
        AnalysisLocation("/test.zep", Span(Position(1, 1), Position(1, 7))),
        Span(Position(1, 8), Position(1, 13)), {field});
    const auto result = protocol.document_symbols(document, {structure});
    const auto& symbol = result[0];
    const auto& child = symbol["children"][0];

    EXPECT_EQ(symbol["kind"], 23);
    EXPECT_EQ(child["kind"], 8);
    EXPECT_EQ(symbol["range"]["end"]["line"], 1);
    EXPECT_GE(symbol["range"]["end"]["character"].get<int>(),
              child["range"]["end"]["character"].get<int>());
    EXPECT_GE(child["range"]["end"]["character"].get<int>(),
              child["selectionRange"]["end"]["character"].get<int>());
}

TEST_F(LspProtocolTest, ConvertsCompletions) {
    Document document("file:///test.zep", 1, "ru");
    auto result = protocol.completions(
        document,
        {Completion("run", CompletionKind::Type::Method, "fn() -> void",
                    Span(Position(1, 1), Position(1, 3)), "run", "0_run")});

    ASSERT_EQ(result["items"].size(), 1U);
    EXPECT_EQ(result["items"][0]["label"], "run");
    EXPECT_EQ(result["items"][0]["kind"], 2);
    EXPECT_EQ(result["items"][0]["detail"], "fn() -> void");
    EXPECT_EQ(result["items"][0]["textEdit"]["newText"], "run");
}

TEST_F(LspProtocolTest, DeltaEncodesSemanticTokens) {
    Document document("file:///test.zep", 1, " value\n  call");
    std::vector<SemanticToken> tokens;
    tokens.emplace_back(Span(Position(1, 2), Position(1, 4)), SemanticKind::Type::Variable,
                        SemanticModifier::Declaration);
    tokens.emplace_back(Span(Position(2, 3), Position(2, 5)), SemanticKind::Type::Function);

    auto result = protocol.tokens(document, tokens);

    EXPECT_EQ(result["data"], std::vector<std::uint32_t>({0, 1, 3, 7, 1, 1, 2, 3, 10, 0}));
}

TEST(LspDocument, AppliesIncrementalUtf16Changes) {
    Document document("file:///test.zep", 1, "a😀b");
    const auto changes = nlohmann::json::array(
        {{{"range", {{"start", {{"line", 0}, {"character", 1}}},
                       {"end", {{"line", 0}, {"character", 3}}}}},
          {"text", "value"}}});

    EXPECT_TRUE(document.change(2, changes));
    EXPECT_EQ(document.text, "avalueb");
    EXPECT_EQ(document.version, 2);
    EXPECT_FALSE(document.change(2, nlohmann::json::array({{{"text", "stale"}}})));
    EXPECT_EQ(document.text, "avalueb");
}

TEST(LspDocument, EncodesAndDecodesFileUris) {
    const auto path = document_path("file:///tmp/source%20file.zep");

    EXPECT_EQ(path, "/tmp/source file.zep");
    EXPECT_EQ(document_uri(path), "file:///tmp/source%20file.zep");
}
