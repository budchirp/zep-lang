module;

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

export module zep.lsp.features.diagnostics;

import zep.lsp.protocol.message;
import zep.lsp.protocol.types;
import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class DiagnosticsFeature {
  private:
    static void publish_diagnostics(ServerContext& context, const std::string& uri,
                                    const std::vector<lsp::Diagnostic>& diagnostics) {
        nlohmann::json diagnostic_array = nlohmann::json::array();
        for (const auto& diagnostic : diagnostics) {
            nlohmann::json item;
            item["range"] = {{"start",
                              {{"line", diagnostic.range.start.line},
                               {"character", diagnostic.range.start.character}}},
                             {"end",
                              {{"line", diagnostic.range.end.line},
                               {"character", diagnostic.range.end.character}}}};
            item["severity"] = static_cast<std::int32_t>(diagnostic.severity);
            item["message"] = diagnostic.message;
            item["source"] = diagnostic.source;
            if (!diagnostic.code.empty()) {
                item["code"] = diagnostic.code;
            }
            diagnostic_array.push_back(std::move(item));
        }

        nlohmann::json params;
        params["uri"] = uri;
        params["diagnostics"] = diagnostic_array;

        Notification notification("textDocument/publishDiagnostics", std::move(params));
        context.transport.write_message(notification.to_json());
    }

  public:
    static void register_feature(Dispatcher& dispatcher, ServerContext& context) {
        dispatcher.register_notification("textDocument/didOpen",
                                         [&context](const nlohmann::json& params) {
                                             if (!params.contains("textDocument")) {
                                                 return;
                                             }

                                             const auto& document_json = params["textDocument"];
                                             auto uri = document_json["uri"].get<std::string>();
                                             auto version = document_json.value("version", 0);
                                             auto text = document_json["text"].get<std::string>();

                                             context.documents.open(uri, version, text);
                                             auto diagnostics = context.analysis.analyze(uri, text);
                                             publish_diagnostics(context, uri, diagnostics);
                                         });

        dispatcher.register_notification(
            "textDocument/didChange", [&context](const nlohmann::json& params) {
                if (!params.contains("textDocument") || !params.contains("contentChanges")) {
                    return;
                }

                const auto& document_json = params["textDocument"];
                auto uri = document_json["uri"].get<std::string>();
                auto version = document_json.value("version", 0);

                const auto& changes = params["contentChanges"];
                if (changes.empty()) {
                    return;
                }

                auto text = changes.front()["text"].get<std::string>();
                context.documents.update(uri, version, text);

                auto diagnostics = context.analysis.analyze(uri, text);
                publish_diagnostics(context, uri, diagnostics);
            });

        dispatcher.register_notification(
            "textDocument/didClose", [&context](const nlohmann::json& params) {
                if (!params.contains("textDocument")) {
                    return;
                }

                auto uri = params["textDocument"]["uri"].get<std::string>();
                context.documents.close(uri);
                context.analysis.close_document(uri);
                publish_diagnostics(context, uri, {});
            });
    }
};
