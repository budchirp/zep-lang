module;

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

export module zep.lsp.features.completion;

import zep.lsp.protocol.types;
import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class CompletionFeature {
  private:
  public:
    static void register_feature(Dispatcher& dispatcher, ServerContext& context) {
        dispatcher.register_request(
            "textDocument/completion", [&context](const nlohmann::json& params) -> nlohmann::json {
                std::string uri;
                std::string content;
                lsp::Position position(0, 0);

                if (params.contains("textDocument") && params["textDocument"].contains("uri")) {
                    uri = params["textDocument"]["uri"].get<std::string>();
                    const auto* document = context.documents.find(uri);
                    if (document != nullptr) {
                        content = document->content;
                    }
                }

                if (params.contains("position")) {
                    const auto& position_json = params["position"];
                    auto line = position_json.value("line", 0U);
                    auto character = position_json.value("character", 0U);
                    position = lsp::Position(line, character);
                }

                auto items = context.analysis.complete(uri, content, position);

                nlohmann::json items_array = nlohmann::json::array();
                for (const auto& item : items) {
                    nlohmann::json item_json;
                    item_json["label"] = item.label;
                    item_json["kind"] = static_cast<std::uint32_t>(item.kind);
                    if (!item.detail.empty()) {
                        item_json["detail"] = item.detail;
                    }
                    if (!item.documentation.empty()) {
                        item_json["documentation"] = item.documentation;
                    }

                    items_array.push_back(std::move(item_json));
                }

                nlohmann::json result;
                result["isIncomplete"] = false;
                result["items"] = items_array;

                return result;
            });
    }
};
