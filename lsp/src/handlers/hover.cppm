module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.hover;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_hover(Dispatcher& dispatcher, Session& session, ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/hover",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto uri = parameters.at("textDocument").at("uri").get<std::string>();
        auto* document = session.document(uri);
        if (document == nullptr) {
            return nlohmann::json();
        }

        auto& analysis = session.analysis(uri);
        auto hover = analysis.hover(protocol.position(*document, parameters.at("position")));
        if (!hover.has_value()) {
            return nlohmann::json();
        }

        return nlohmann::json{
            {"contents", {{"kind", "markdown"}, {"value", "```zep\n" + hover->contents + "\n```"}}},
            {"range", protocol.range(*document, hover->span)}};
    });
}
