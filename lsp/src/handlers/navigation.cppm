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
        auto uri = parameters.at("textDocument").at("uri").get<std::string>();
        auto* document = session.document(uri);
        if (document == nullptr) {
            return nlohmann::json();
        }

        auto location = session.analysis(uri).definition(
            protocol.position(*document, parameters.at("position")));
        return location.has_value() ? protocol.location(*location) : nlohmann::json();
    });

    dispatcher.on_request("textDocument/references",
                          [&session, &protocol](const nlohmann::json& parameters) {
        auto uri = parameters.at("textDocument").at("uri").get<std::string>();
        auto* document = session.document(uri);
        nlohmann::json result = nlohmann::json::array();
        if (document == nullptr) {
            return result;
        }

        auto include_declaration = parameters.at("context").value("includeDeclaration", false);
        auto references = session.analysis(uri).references(
            protocol.position(*document, parameters.at("position")), include_declaration);
        for (const auto& location : references) {
            result.push_back(protocol.location(location));
        }
        return result;
    });

    dispatcher.on_request("textDocument/documentHighlight",
                          [&session, &protocol](const nlohmann::json& parameters) {
                              auto uri = parameters.at("textDocument").at("uri").get<std::string>();
                              auto* document = session.document(uri);
                              if (document == nullptr) {
                                  return nlohmann::json::array();
                              }
                              auto position = protocol.position(*document, parameters.at("position"));
                              return protocol.highlights(
                                  *document, session.analysis(uri).highlights(position));
                          });
}
