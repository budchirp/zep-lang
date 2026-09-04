#include <gtest/gtest.h>
#include <string>

import zep.common.source.position;
import zep.common.source.span;
import zep.lsp.document;
import zep.lsp.document.store;
import zep.lsp.protocol.types;

TEST(LspDocumentStore, OpensUpdatesAndClosesDocuments) {
    DocumentStore store;

    EXPECT_EQ(store.size(), 0U);
    EXPECT_EQ(store.find("file:///test.zep"), nullptr);

    store.open("file:///test.zep", 1, "var x = 1;");
    EXPECT_EQ(store.size(), 1U);

    const auto* doc = store.find("file:///test.zep");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 1);
    EXPECT_EQ(doc->content, "var x = 1;");

    store.update("file:///test.zep", 2, "var x = 2;");
    doc = store.find("file:///test.zep");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->content, "var x = 2;");

    store.close("file:///test.zep");
    EXPECT_EQ(store.size(), 0U);
    EXPECT_EQ(store.find("file:///test.zep"), nullptr);
}

TEST(LspPositionConversion, ConvertsBetweenCompilerAndLspPositions) {
    ::Position compiler_pos(1, 1);
    auto lsp_pos = lsp::PositionConverter::from_compiler(compiler_pos);
    EXPECT_EQ(lsp_pos.line, 0U);
    EXPECT_EQ(lsp_pos.character, 0U);

    auto roundtrip = lsp::PositionConverter::to_compiler(lsp_pos);
    EXPECT_EQ(roundtrip.line, 1U);
    EXPECT_EQ(roundtrip.column, 1U);

    ::Position compiler_pos2(10, 25);
    auto lsp_pos2 = lsp::PositionConverter::from_compiler(compiler_pos2);
    EXPECT_EQ(lsp_pos2.line, 9U);
    EXPECT_EQ(lsp_pos2.character, 24U);

    auto roundtrip2 = lsp::PositionConverter::to_compiler(lsp_pos2);
    EXPECT_EQ(roundtrip2.line, 10U);
    EXPECT_EQ(roundtrip2.column, 25U);
}

TEST(LspPositionConversion, ConvertsCompilerSpanToLspRange) {
    Span span(::Position(2, 5), ::Position(2, 15));
    auto range = lsp::PositionConverter::from_compiler_span(span);

    EXPECT_EQ(range.start.line, 1U);
    EXPECT_EQ(range.start.character, 4U);
    EXPECT_EQ(range.end.line, 1U);
    EXPECT_EQ(range.end.character, 14U);
}
