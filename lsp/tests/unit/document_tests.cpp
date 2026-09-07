#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

import zep.lsp.document;

TEST(LspDocument, AppliesIncrementalUtf16Changes) {
    Document document("file:///test.zep", 1, "a😀b");
    nlohmann::json changes = nlohmann::json::array(
        {{{"range", {{"start", {{"line", 0}, {"character", 1}}},
                       {"end", {{"line", 0}, {"character", 3}}}}},
          {"text", "value"}}});

    EXPECT_TRUE(document.change(2, changes));
    EXPECT_EQ(document.text, "avalueb");
    EXPECT_EQ(document.version, 2);
}

TEST(LspDocument, IgnoresStaleVersions) {
    Document document("file:///test.zep", 4, "old");
    nlohmann::json changes = nlohmann::json::array({{{"text", "new"}}});

    EXPECT_FALSE(document.change(4, changes));
    EXPECT_EQ(document.text, "old");
}

TEST(LspDocument, EncodesAndDecodesFileUris) {
    auto path = document_path("file:///tmp/source%20file.zep");

    EXPECT_EQ(path, "/tmp/source file.zep");
    EXPECT_EQ(document_uri(path), "file:///tmp/source%20file.zep");
}
