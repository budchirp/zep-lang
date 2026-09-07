module;

#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

export module zep.lsp.handlers.lifecycle;

import zep.lsp.dispatcher;
import zep.lsp.session;

export void register_lifecycle(Dispatcher& dispatcher, Session& session) {
    dispatcher.on_request("initialize", [&session](const nlohmann::json& parameters) {
        if (session.state != Session::State::Created) {
            throw std::invalid_argument("server is already initialized");
        }
        std::optional<std::string> root_uri;
        if (parameters.contains("rootUri") && parameters.at("rootUri").is_string()) {
            root_uri = parameters.at("rootUri").get<std::string>();
        }
        session.initialize(std::move(root_uri));
        return nlohmann::json{
            {"capabilities",
             {{"textDocumentSync", {{"openClose", true}, {"change", 2}}},
              {"hoverProvider", true},
              {"completionProvider", {{"triggerCharacters", {".", ":"}}}},
              {"definitionProvider", true},
              {"referencesProvider", true},
              {"documentHighlightProvider", true},
              {"documentSymbolProvider", true},
              {"workspaceSymbolProvider", true},
              {"signatureHelpProvider", {{"triggerCharacters", {"(", ","}}}},
              {"semanticTokensProvider",
               {{"legend",
                 {{"tokenTypes",
                   {"type", "class", "enum", "interface", "struct", "typeParameter", "parameter",
                    "variable", "property", "enumMember", "function", "method", "keyword",
                    "modifier", "comment", "string", "number", "operator"}},
                  {"tokenModifiers",
                   {"declaration", "definition", "readonly", "static", "defaultLibrary"}}}},
                {"full", true},
                {"range", true}}}}}};
    });

    dispatcher.on_notification("initialized", [](const nlohmann::json&) {});
    dispatcher.on_request("shutdown", [&session](const nlohmann::json&) {
        if (session.state != Session::State::Initialized) {
            throw std::invalid_argument("server is not initialized");
        }
        session.state = Session::State::Shutdown;
        return nlohmann::json();
    });
    dispatcher.on_notification("exit", [&session](const nlohmann::json&) {
        session.exit_code = session.state == Session::State::Shutdown ? 0 : 1;
        session.state = Session::State::Exited;
    });
}
