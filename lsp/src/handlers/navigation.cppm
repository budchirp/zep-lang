module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.handlers.navigation;

import zep.lsp.dispatcher;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_navigation(Dispatcher& dispatcher, Session& session,
                                ProtocolCodec& protocol) {
    dispatcher.on_request("textDocument/definition",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto request = protocol.request_position(parameters);

        if (!request.has_value()) {
            return nlohmann::json();
        }

        auto location = session.analysis(request->uri).definition(request->position);
        return location.has_value() ? protocol.location(*location) : nlohmann::json();
    });

    dispatcher.on_request("textDocument/references",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto request = protocol.request_position(parameters);
        nlohmann::json result = nlohmann::json::array();

        if (!request.has_value()) {
            return result;
        }

        auto include_declaration = parameters.at("context").value("includeDeclaration", false);
        auto references = session.analysis(request->uri).references(request->position, include_declaration);
        for (const auto& location : references) {
            result.push_back(protocol.location(location));
        }
        return result;
    });

    dispatcher.on_request("textDocument/documentHighlight",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto request = protocol.request_position(parameters);

                              if (!request.has_value()) {
                                  return nlohmann::json::array();
                              }
                              return protocol.highlights(
                                  request->document, session.analysis(request->uri).highlights(request->position));
                          });
}
