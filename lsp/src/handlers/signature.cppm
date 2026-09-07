module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.signature;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_signature(Dispatcher& dispatcher, Session& session,
                               ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/signatureHelp",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto uri = parameters.at("textDocument").at("uri").get<std::string>();
                              auto* document = session.document(uri);
                              if (document == nullptr) {
                                  return nlohmann::json();
                              }
                              auto help = session.analysis(uri).signature(
                                  protocol.position(*document, parameters.at("position")));
                              return help.has_value() ? protocol.signature(*help)
                                                      : nlohmann::json();
                          });
}
