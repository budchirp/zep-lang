module;

#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.features.tokens;

import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class SemanticTokensFeature {
  private:
  public:
    static void register_feature(Dispatcher& dispatcher, ServerContext& context) {
        dispatcher.register_request(
            "textDocument/semanticTokens/full", [&context](const nlohmann::json& params) {
                auto uri = params["textDocument"]["uri"].get<std::string>();
                auto* document = context.documents.find(uri);
                if (document == nullptr) {
                    return nlohmann::json{{"data", nlohmann::json::array()}};
                }

                auto tokens = context.analysis.semantic_tokens(uri, document->content);
                nlohmann::json result;
                result["data"] = tokens.data;
                return result;
            });
    }
};
