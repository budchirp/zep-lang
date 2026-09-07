#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
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
