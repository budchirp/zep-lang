#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

import zep.lsp.transport;

TEST(LspTransport, ReadsFramedMessage) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}";
    std::string input = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;

    std::istringstream in(input);
    std::ostringstream out;
    Transport transport(in, out);

    auto message = transport.read();
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ((*message)["id"], 1);
    EXPECT_EQ((*message)["method"], "initialize");
}

TEST(LspTransport, ReadsConsecutiveMessages) {
    std::string first = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"first\"}";
    std::string second = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"second\"}";
    std::string input = "Content-Length: " + std::to_string(first.size()) + "\r\n\r\n" + first +
                        "Content-Length: " + std::to_string(second.size()) + "\r\n\r\n" + second;

    std::istringstream in(input);
    std::ostringstream out;
    Transport transport(in, out);

    auto first_message = transport.read();
    ASSERT_TRUE(first_message.has_value());
    EXPECT_EQ((*first_message)["id"], 1);

    auto second_message = transport.read();
    ASSERT_TRUE(second_message.has_value());
    EXPECT_EQ((*second_message)["id"], 2);
}

TEST(LspTransport, WritesFramedMessage) {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);

    nlohmann::json payload;
    payload["jsonrpc"] = "2.0";
    payload["id"] = 42;
    payload["result"] = "ok";

    transport.write(payload);

    std::string expected_body = payload.dump();
    std::string expected =
        "Content-Length: " + std::to_string(expected_body.size()) + "\r\n\r\n" + expected_body;
    EXPECT_EQ(out.str(), expected);
}

TEST(LspTransport, HandlesMalformedHeaderGracefully) {
    std::string input = "Invalid-Header: none\r\n\r\n";
    std::istringstream in(input);
    std::ostringstream out;
    Transport transport(in, out);

    auto message = transport.read();
    EXPECT_FALSE(message.has_value());
    EXPECT_EQ(transport.read_status, Transport::ReadStatus::InvalidFrame);
}

TEST(LspTransport, DistinguishesInvalidJson) {
    std::string payload = "{";
    std::string input = "Content-Length: 1\r\n\r\n" + payload;
    std::istringstream stream(input);
    std::ostringstream output;
    Transport transport(stream, output);

    EXPECT_FALSE(transport.read().has_value());
    EXPECT_EQ(transport.read_status, Transport::ReadStatus::InvalidJson);
}
