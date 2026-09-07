module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.symbols;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_symbols(Dispatcher& dispatcher, Session& session, ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/documentSymbol",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto uri = parameters.at("textDocument").at("uri").get<std::string>();
                              auto* document = session.document(uri);
                              return document != nullptr
                                         ? protocol.document_symbols(
                                               *document,
                                               session.analysis(uri).document_symbols())
                                         : nlohmann::json::array();
                          });

    dispatcher.on_request("workspace/symbol",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto symbols = session.workspace_symbols(parameters.value("query", std::string()));
        return protocol.workspace_symbols(symbols);
    });
}
