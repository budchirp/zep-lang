module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.completion;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_completion(Dispatcher& dispatcher, Session& session,
                                ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/completion",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto request = protocol.request_position(parameters);

        if (!request.has_value()) {
            return nlohmann::json{{"isIncomplete", false}, {"items", nlohmann::json::array()}};
        }

        return protocol.completions(request->document, session.complete(request->uri, request->position));
    });
}
