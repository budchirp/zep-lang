module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.tokens;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;
import zep.common.source.span;

export void register_tokens(Dispatcher& dispatcher, Session& session, ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/semanticTokens/full",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto uri = parameters.at("textDocument").at("uri").get<std::string>();
                              auto* document = session.document(uri);
                              if (document == nullptr) {
                                  return nlohmann::json{{"data", nlohmann::json::array()}};
                              }

                              return protocol.tokens(*document, session.analysis(uri).tokens());
                          });

    dispatcher.on_request("textDocument/semanticTokens/range",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto uri = parameters.at("textDocument").at("uri").get<std::string>();
                              auto* document = session.document(uri);
                              if (document == nullptr) {
                                  return nlohmann::json{{"data", nlohmann::json::array()}};
                              }
                              auto start = protocol.position(
                                  *document, parameters.at("range").at("start"));
                              auto end = protocol.position(
                                  *document, parameters.at("range").at("end"));
                              return protocol.tokens(
                                  *document, session.analysis(uri).tokens(Span(start, end)));
                          });
}
