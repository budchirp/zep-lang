#include <csignal>
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

import zep.lsp.transport;

TEST(LspIntegration, FullClientServerLifecycle) {
    int stdin_pipe[2];
    int stdout_pipe[2];

    ASSERT_EQ(pipe(stdin_pipe), 0);
    ASSERT_EQ(pipe(stdout_pipe), 0);

    auto pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        execl(ZEP_ZEP_EXECUTABLE, ZEP_ZEP_EXECUTABLE, "lsp", nullptr);
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    auto frame_message = [](const nlohmann::json& payload) {
        auto body = payload.dump();
        return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    };

    std::string request_stream;

    nlohmann::json initialize_request;
    initialize_request["jsonrpc"] = "2.0";
    initialize_request["id"] = 1;
    initialize_request["method"] = "initialize";
    initialize_request["params"] = {{"capabilities", nlohmann::json::object()}};
    request_stream += frame_message(initialize_request);

    nlohmann::json initialized_notification;
    initialized_notification["jsonrpc"] = "2.0";
    initialized_notification["method"] = "initialized";
    initialized_notification["params"] = nlohmann::json::object();
    request_stream += frame_message(initialized_notification);

    nlohmann::json did_open_notification;
    did_open_notification["jsonrpc"] = "2.0";
    did_open_notification["method"] = "textDocument/didOpen";
    did_open_notification["params"] = {
        {"textDocument",
         {{"uri", "file:///main.zep"},
          {"languageId", "zep"},
          {"version", 1},
          {"text", "fn add(a: i32, b: i32) -> i32 { return a + b; }\n"
                   "fn main() -> i32 { return add(1, 2); }"}}}};
    request_stream += frame_message(did_open_notification);

    nlohmann::json hover_request;
    hover_request["jsonrpc"] = "2.0";
    hover_request["id"] = 2;
    hover_request["method"] = "textDocument/hover";
    hover_request["params"] = {{"textDocument", {{"uri", "file:///main.zep"}}},
                               {"position", {{"line", 0}, {"character", 4}}}};
    request_stream += frame_message(hover_request);

    nlohmann::json completion_request;
    completion_request["jsonrpc"] = "2.0";
    completion_request["id"] = 3;
    completion_request["method"] = "textDocument/completion";
    completion_request["params"] = {{"textDocument", {{"uri", "file:///main.zep"}}},
                                    {"position", {{"line", 0}, {"character", 0}}}};
    request_stream += frame_message(completion_request);

    nlohmann::json tokens_request;
    tokens_request["jsonrpc"] = "2.0";
    tokens_request["id"] = 4;
    tokens_request["method"] = "textDocument/semanticTokens/full";
    tokens_request["params"] = {{"textDocument", {{"uri", "file:///main.zep"}}}};
    request_stream += frame_message(tokens_request);

    for (const auto& [identifier, method] :
         std::vector<std::pair<int, std::string>>{{5, "textDocument/definition"},
                                                  {6, "textDocument/references"},
                                                  {7, "textDocument/documentHighlight"},
                                                  {9, "textDocument/signatureHelp"}}) {
        nlohmann::json request = {{"jsonrpc", "2.0"},
                                  {"id", identifier},
                                  {"method", method},
                                  {"params",
                                   {{"textDocument", {{"uri", "file:///main.zep"}}},
                                    {"position", {{"line", 1}, {"character", 29}}}}}};
        if (method == "textDocument/references") {
            request["params"]["context"] = {{"includeDeclaration", true}};
        }
        request_stream += frame_message(request);
    }

    request_stream += frame_message({{"jsonrpc", "2.0"},
                                     {"id", 8},
                                     {"method", "textDocument/documentSymbol"},
                                     {"params", {{"textDocument", {{"uri", "file:///main.zep"}}}}}});
    request_stream += frame_message(
        {{"jsonrpc", "2.0"},
         {"id", 10},
         {"method", "textDocument/semanticTokens/range"},
         {"params",
          {{"textDocument", {{"uri", "file:///main.zep"}}},
           {"range", {{"start", {{"line", 0}, {"character", 0}}},
                      {"end", {{"line", 1}, {"character", 38}}}}}}}});

    request_stream += frame_message(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didChange"},
         {"params",
          {{"textDocument", {{"uri", "file:///main.zep"}, {"version", 2}}},
           {"contentChanges",
            {{{"range", {{"start", {{"line", 1}, {"character", 30}}},
                         {"end", {{"line", 1}, {"character", 31}}}}},
              {"text", "3"}}}}}}});

    nlohmann::json shutdown_request;
    shutdown_request["jsonrpc"] = "2.0";
    shutdown_request["id"] = 11;
    shutdown_request["method"] = "shutdown";
    request_stream += frame_message(shutdown_request);

    nlohmann::json exit_notification;
    exit_notification["jsonrpc"] = "2.0";
    exit_notification["method"] = "exit";
    request_stream += frame_message(exit_notification);

    auto written = write(stdin_pipe[1], request_stream.data(), request_stream.size());
    EXPECT_EQ(written, static_cast<ssize_t>(request_stream.size()));
    close(stdin_pipe[1]);

    std::string response_data;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        response_data.append(buffer, static_cast<std::size_t>(bytes_read));
    }
    close(stdout_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);

    std::istringstream response_stream(response_data);
    std::ostringstream dummy_out;
    Transport transport(response_stream, dummy_out);

    std::vector<nlohmann::json> messages;
    while (auto msg = transport.read()) {
        messages.push_back(std::move(*msg));
    }

    ASSERT_GE(messages.size(), 13U);

    EXPECT_EQ(messages[0]["id"], 1);
    ASSERT_TRUE(messages[0].contains("result"));
    EXPECT_TRUE(messages[0]["result"]["capabilities"]["hoverProvider"].get<bool>());
    EXPECT_EQ(messages[0]["result"]["capabilities"]["textDocumentSync"]["change"].get<int>(), 2);
    EXPECT_TRUE(messages[0]["result"]["capabilities"].contains("semanticTokensProvider"));
    EXPECT_TRUE(messages[0]["result"]["capabilities"]["definitionProvider"].get<bool>());
    EXPECT_TRUE(messages[0]["result"]["capabilities"]["signatureHelpProvider"].is_object());

    EXPECT_EQ(messages[1]["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(messages[1]["params"]["uri"], "file:///main.zep");
    EXPECT_TRUE(messages[1]["params"]["diagnostics"].empty());

    EXPECT_EQ(messages[2]["id"], 2);
    ASSERT_TRUE(messages[2].contains("result"));
    EXPECT_NE(messages[2]["result"]["contents"]["value"].get<std::string>().find("add"),
              std::string::npos);

    EXPECT_EQ(messages[3]["id"], 3);
    ASSERT_TRUE(messages[3].contains("result"));
    EXPECT_FALSE(messages[3]["result"]["items"].empty());

    EXPECT_EQ(messages[4]["id"], 4);
    ASSERT_TRUE(messages[4].contains("result"));
    EXPECT_TRUE(messages[4]["result"].contains("data"));
    EXPECT_FALSE(messages[4]["result"]["data"].empty());

    for (auto identifier = 5; identifier <= 10; ++identifier) {
        auto response = std::ranges::find_if(messages, [identifier](const nlohmann::json& message) {
            return message.value("id", -1) == identifier;
        });
        ASSERT_NE(response, messages.end());
        EXPECT_TRUE(response->contains("result")) << response->dump();
    }

    auto shutdown_response = std::ranges::find_if(messages, [](const nlohmann::json& message) {
        return message.value("id", -1) == 11;
    });
    ASSERT_NE(shutdown_response, messages.end());
    EXPECT_TRUE((*shutdown_response)["result"].is_null());

    auto updated_diagnostics = std::ranges::find_if(messages, [](const nlohmann::json& message) {
        return message.value("method", std::string()) == "textDocument/publishDiagnostics" &&
               message.at("params").value("version", 0) == 2;
    });
    EXPECT_NE(updated_diagnostics, messages.end());
}

TEST(LspIntegration, CliZepLspSubcommand) {
    int stdin_pipe[2];
    int stdout_pipe[2];

    ASSERT_EQ(pipe(stdin_pipe), 0);
    ASSERT_EQ(pipe(stdout_pipe), 0);

    auto pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        execl(ZEP_ZEP_EXECUTABLE, ZEP_ZEP_EXECUTABLE, "lsp", nullptr);
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    auto frame_message = [](const nlohmann::json& payload) {
        auto body = payload.dump();
        return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    };

    std::string request_stream;

    nlohmann::json initialize_request;
    initialize_request["jsonrpc"] = "2.0";
    initialize_request["id"] = 1;
    initialize_request["method"] = "initialize";
    initialize_request["params"] = {{"capabilities", nlohmann::json::object()}};
    request_stream += frame_message(initialize_request);

    nlohmann::json shutdown_request;
    shutdown_request["jsonrpc"] = "2.0";
    shutdown_request["id"] = 2;
    shutdown_request["method"] = "shutdown";
    request_stream += frame_message(shutdown_request);

    nlohmann::json exit_notification;
    exit_notification["jsonrpc"] = "2.0";
    exit_notification["method"] = "exit";
    request_stream += frame_message(exit_notification);

    auto written = write(stdin_pipe[1], request_stream.data(), request_stream.size());
    EXPECT_EQ(written, static_cast<ssize_t>(request_stream.size()));
    close(stdin_pipe[1]);

    std::string response_data;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        response_data.append(buffer, static_cast<std::size_t>(bytes_read));
    }
    close(stdout_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);

    std::istringstream response_stream(response_data);
    std::ostringstream dummy_out;
    Transport transport(response_stream, dummy_out);

    std::vector<nlohmann::json> messages;
    while (auto msg = transport.read()) {
        messages.push_back(std::move(*msg));
    }

    ASSERT_GE(messages.size(), 2U);
    EXPECT_EQ(messages[0]["id"], 1);
    EXPECT_TRUE(messages[0].contains("result"));
    EXPECT_TRUE(messages[0]["result"]["capabilities"]["hoverProvider"].get<bool>());
    EXPECT_EQ(messages[1]["id"], 2);
    EXPECT_TRUE(messages[1]["result"].is_null());
}
