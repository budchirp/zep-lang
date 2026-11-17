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
        auto request = protocol.request_position(parameters);

        if (!request.has_value()) {
            return nlohmann::json();
        }

        auto hover = session.analysis(request->uri).hover(request->position);
        if (!hover.has_value()) {
            return nlohmann::json();
        }

        return nlohmann::json{
            {"contents", {{"kind", "markdown"}, {"value", "```zep\n" + hover->contents + "\n```"}}},
            {"range", protocol.range(request->document, hover->span)}};
    });
}
