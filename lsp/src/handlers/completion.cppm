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
        auto uri = parameters.at("textDocument").at("uri").get<std::string>();
        auto* document = session.document(uri);
        if (document == nullptr) {
            return nlohmann::json{{"isIncomplete", false}, {"items", nlohmann::json::array()}};
        }

        const auto& requested_position = parameters.at("position");
        auto position = protocol.position(*document, requested_position);
        return protocol.completions(*document, session.complete(uri, position));
    });
}
