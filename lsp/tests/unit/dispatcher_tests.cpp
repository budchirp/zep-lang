#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

import zep.lsp.dispatcher;

TEST(LspDispatcher, RoutesRequestsAndPreservesIds) {
    Dispatcher dispatcher;
    dispatcher.on_request("custom/echo",
                          [](const nlohmann::json& parameters) { return parameters; });

    auto integer_response = dispatcher.dispatch({{"jsonrpc", "2.0"},
                                                 {"id", 1},
                                                 {"method", "custom/echo"},
                                                 {"params", {{"text", "hello"}}}});
    ASSERT_TRUE(integer_response.has_value());
    EXPECT_EQ((*integer_response)["id"], 1);
    EXPECT_EQ((*integer_response)["result"]["text"], "hello");

    auto string_response =
        dispatcher.dispatch({{"jsonrpc", "2.0"}, {"id", "request"}, {"method", "custom/echo"}});
    ASSERT_TRUE(string_response.has_value());
    EXPECT_EQ((*string_response)["id"], "request");
}

TEST(LspDispatcher, ReturnsMethodNotFound) {
    Dispatcher dispatcher;
    auto response =
        dispatcher.dispatch({{"jsonrpc", "2.0"}, {"id", 5}, {"method", "unknown/method"}});

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["id"], 5);
    EXPECT_EQ((*response)["error"]["code"], -32601);
}

TEST(LspDispatcher, RoutesNotificationsWithoutResponse) {
    Dispatcher dispatcher;
    std::string received_value;
    dispatcher.on_notification("custom/notify", [&](const nlohmann::json& parameters) {
        received_value = parameters.at("value").get<std::string>();
    });

    auto response = dispatcher.dispatch(
        {{"jsonrpc", "2.0"}, {"method", "custom/notify"}, {"params", {{"value", "ping"}}}});

    EXPECT_FALSE(response.has_value());
    EXPECT_EQ(received_value, "ping");
}

TEST(LspDispatcher, IgnoresUnknownNotifications) {
    Dispatcher dispatcher;
    auto response = dispatcher.dispatch({{"jsonrpc", "2.0"}, {"method", "unknown/notify"}});
    EXPECT_FALSE(response.has_value());
}

TEST(LspDispatcher, RejectsInvalidJsonRpcVersion) {
    Dispatcher dispatcher;
    auto response = dispatcher.dispatch({{"jsonrpc", "1.0"}, {"id", 3}, {"method", "test"}});

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["error"]["code"], -32600);
}

TEST(LspDispatcher, RejectsDuplicateRegistration) {
    Dispatcher dispatcher;
    dispatcher.on_request("test", [](const nlohmann::json&) { return nlohmann::json(); });

    EXPECT_THROW(
        dispatcher.on_request("test", [](const nlohmann::json&) { return nlohmann::json(); }),
        std::invalid_argument);
}
