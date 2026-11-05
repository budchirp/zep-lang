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
                              auto request = protocol.request_position(parameters);

                              if (!request.has_value()) {
                                  return nlohmann::json();
                              }
                              auto help = session.analysis(request->uri).signature(request->position);
                              return help.has_value() ? protocol.signature(*help)
                                                      : nlohmann::json();
                          });
}
