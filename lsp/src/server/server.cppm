module;

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

export module zep.lsp.server;

import zep.lsp.analysis;
import zep.lsp.document.store;
import zep.lsp.features.completion;
import zep.lsp.features.diagnostics;
import zep.lsp.features.hover;
import zep.lsp.features.initialize;
import zep.lsp.features.tokens;
import zep.lsp.protocol.message;
import zep.lsp.protocol.transport;
import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class Server {
  private:
    void register_features() {
        InitializeFeature::register_feature(dispatcher, context);
        DiagnosticsFeature::register_feature(dispatcher, context);
        HoverFeature::register_feature(dispatcher, context);
        CompletionFeature::register_feature(dispatcher, context);
        SemanticTokensFeature::register_feature(dispatcher, context);
    }

  public:
    DocumentStore documents;
    AnalysisService analysis;
    Transport transport;
    ServerContext context;
    Dispatcher dispatcher;

    Server(std::istream& input, std::ostream& output)
        : documents(), analysis(), transport(input, output),
          context(documents, analysis, transport), dispatcher() {
        register_features();
    }

    int run() {
        while (!context.is_exit) {
            auto message_optional = transport.read_message();
            if (!message_optional.has_value()) {
                break;
            }

            const auto& message = *message_optional;
            if (!message.is_object()) {
                continue;
            }

            if (message.contains("id")) {
                auto id = JsonRpcId::from_json(message["id"]);
                auto method = message.value("method", "");
                auto params = message.contains("params") ? message["params"] : nlohmann::json();

                Request request(std::move(id), std::move(method), std::move(params));
                auto response_json = dispatcher.dispatch_request(request);
                transport.write_message(response_json);
            } else if (message.contains("method")) {
                auto method = message["method"].get<std::string>();
                auto params = message.contains("params") ? message["params"] : nlohmann::json();

                Notification notification(std::move(method), std::move(params));
                dispatcher.dispatch_notification(notification);
            }
        }

        return context.exit_code;
    }
};
