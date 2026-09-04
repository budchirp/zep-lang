#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

import zep.lsp.protocol.message;
import zep.lsp.server.dispatcher;

TEST(LspDispatcher, RoutesRegisteredRequests) {
    Dispatcher dispatcher;
    dispatcher.register_request("custom/echo", [](const nlohmann::json& params) { return params; });

    nlohmann::json params;
    params["text"] = "hello";

    Request request(JsonRpcId(1), "custom/echo", params);
    auto response = dispatcher.dispatch_request(request);

    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["result"]["text"], "hello");
}

TEST(LspDispatcher, ReturnsMethodNotFoundForUnknownRequest) {
    Dispatcher dispatcher;

    Request request(JsonRpcId(5), "unknown/method");
    auto response = dispatcher.dispatch_request(request);

    EXPECT_EQ(response["id"], 5);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32601);
}

TEST(LspDispatcher, RoutesRegisteredNotifications) {
    Dispatcher dispatcher;
    bool called = false;
    std::string received_value;

    dispatcher.register_notification("custom/notify", [&](const nlohmann::json& params) {
        called = true;
        received_value = params["value"].get<std::string>();
    });

    nlohmann::json params;
    params["value"] = "ping";

    Notification notification("custom/notify", params);
    dispatcher.dispatch_notification(notification);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_value, "ping");
}

TEST(LspDispatcher, IgnoresUnknownNotificationsWithoutError) {
    Dispatcher dispatcher;

    Notification notification("unknown/notify");
    EXPECT_NO_THROW(dispatcher.dispatch_notification(notification));
}
