module;

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

export module zep.lsp.handlers.documents;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

namespace {
void publish(Session& session, ProtocolCodec& protocol, const std::string& uri) {
    auto* document = session.document(uri);
    if (document == nullptr) {
        return;
    }
    auto& analysis = session.analysis(uri);
    session.write(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/publishDiagnostics"},
         {"params",
          {{"uri", uri},
           {"version", document->version},
           {"diagnostics", protocol.diagnostics(*document, analysis.diagnostics())}}}});
}
}

export void register_documents(Dispatcher& dispatcher, Session& session,
                               ProtocolCodec& protocol) {
    dispatcher.on_notification("textDocument/didOpen",
                               [&session, &protocol](const nlohmann::json& parameters) {
                                   const auto& document = parameters.at("textDocument");
                                   auto uri = document.at("uri").get<std::string>();
                                   session.open(uri, document.at("version").get<std::int64_t>(),
                                                document.at("text").get<std::string>());
                                   publish(session, protocol, uri);
                               });

    dispatcher.on_notification(
        "textDocument/didChange", [&session, &protocol](const nlohmann::json& parameters) {
            const auto& document = parameters.at("textDocument");
            auto uri = document.at("uri").get<std::string>();
            if (!session.change(uri, document.at("version").get<std::int64_t>(),
                                parameters.at("contentChanges"))) {
                return;
            }
            for (const auto* opened : session.open_documents()) {
                publish(session, protocol, opened->uri);
            }
        });

    dispatcher.on_notification(
        "textDocument/didClose", [&session](const nlohmann::json& parameters) {
            auto uri = parameters.at("textDocument").at("uri").get<std::string>();
            session.close(uri);
            session.write({{"jsonrpc", "2.0"},
                           {"method", "textDocument/publishDiagnostics"},
                           {"params", {{"uri", uri}, {"diagnostics", nlohmann::json::array()}}}});
        });
}
