#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

import zep.lsp.protocol.message;

TEST(LspMessage, ParsesIntegerAndStringIds) {
    JsonRpcId numeric_id(100);
    EXPECT_TRUE(numeric_id.is_number());
    EXPECT_EQ(numeric_id.as_number(), 100);
    EXPECT_EQ(numeric_id.to_json(), 100);

    JsonRpcId string_id("req-abc");
    EXPECT_TRUE(string_id.is_string());
    EXPECT_EQ(string_id.as_string(), "req-abc");
    EXPECT_EQ(string_id.to_json(), "req-abc");

    JsonRpcId null_id;
    EXPECT_TRUE(null_id.is_null());
    EXPECT_TRUE(null_id.to_json().is_null());
}

TEST(LspMessage, SerializesResponse) {
    JsonRpcId id(1);
    nlohmann::json result;
    result["status"] = "ready";

    Response response(id, result);
    auto json = response.to_json();

    EXPECT_EQ(json["jsonrpc"], "2.0");
    EXPECT_EQ(json["id"], 1);
    EXPECT_EQ(json["result"]["status"], "ready");
}

TEST(LspMessage, SerializesErrorResponse) {
    JsonRpcId id("req-1");
    JsonRpcError error(static_cast<int>(JsonRpcErrorCode::Type::MethodNotFound),
                       "Method not found");

    ErrorResponse response(id, error);
    auto json = response.to_json();

    EXPECT_EQ(json["jsonrpc"], "2.0");
    EXPECT_EQ(json["id"], "req-1");
    EXPECT_EQ(json["error"]["code"], -32601);
    EXPECT_EQ(json["error"]["message"], "Method not found");
}

TEST(LspMessage, SerializesNotification) {
    nlohmann::json params;
    params["uri"] = "file:///test.zep";

    Notification notification("textDocument/didClose", params);
    auto json = notification.to_json();

    EXPECT_EQ(json["jsonrpc"], "2.0");
    EXPECT_EQ(json["method"], "textDocument/didClose");
    EXPECT_EQ(json["params"]["uri"], "file:///test.zep");
    EXPECT_FALSE(json.contains("id"));
}
