module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.features.initialize;

import zep.lsp.protocol.types;
import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class InitializeFeature {
  private:
  public:
    static void register_feature(Dispatcher& dispatcher, ServerContext& context) {
        dispatcher.register_request("initialize", [](const nlohmann::json&) {
            nlohmann::json completion_options;
            completion_options["triggerCharacters"] = nlohmann::json::array({".", ":"});

            nlohmann::json legend;
            legend["tokenTypes"] = lsp::semantic_token_types;
            legend["tokenModifiers"] = lsp::semantic_token_modifiers;

            nlohmann::json semantic_tokens_options;
            semantic_tokens_options["legend"] = legend;
            semantic_tokens_options["full"] = true;

            nlohmann::json capabilities;
            capabilities["textDocumentSync"] = 1;
            capabilities["hoverProvider"] = true;
            capabilities["completionProvider"] = completion_options;
            capabilities["semanticTokensProvider"] = semantic_tokens_options;

            nlohmann::json result;
            result["capabilities"] = capabilities;

            return result;
        });

        dispatcher.register_notification("initialized", [](const nlohmann::json&) {});

        dispatcher.register_request("shutdown", [&context](const nlohmann::json&) {
            context.is_shutdown = true;
            return nullptr;
        });

        dispatcher.register_notification("exit", [&context](const nlohmann::json&) {
            context.is_exit = true;
            context.exit_code = context.is_shutdown ? 0 : 1;
        });
    }
};
