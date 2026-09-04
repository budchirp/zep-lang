module;

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

export module zep.lsp.features.hover;

import zep.lsp.protocol.types;
import zep.lsp.server.context;
import zep.lsp.server.dispatcher;

export class HoverFeature {
  private:
  public:
    static void register_feature(Dispatcher& dispatcher, ServerContext& context) {
        dispatcher.register_request(
            "textDocument/hover", [&context](const nlohmann::json& params) -> nlohmann::json {
                if (!params.contains("textDocument") || !params.contains("position")) {
                    return nullptr;
                }

                auto uri = params["textDocument"]["uri"].get<std::string>();
                const auto* document = context.documents.find(uri);
                if (document == nullptr) {
                    return nullptr;
                }

                const auto& position_json = params["position"];
                auto line = position_json.value("line", 0U);
                auto character = position_json.value("character", 0U);

                lsp::Position position(line, character);
                auto result = context.analysis.hover(uri, document->content, position);
                if (!result.has_value()) {
                    return nullptr;
                }

                nlohmann::json contents;
                contents["kind"] = "markdown";
                contents["value"] = result->contents;

                nlohmann::json hover_json;
                hover_json["contents"] = contents;

                if (result->range.has_value()) {
                    hover_json["range"] = {{"start",
                                            {{"line", result->range->start.line},
                                             {"character", result->range->start.character}}},
                                           {"end",
                                            {{"line", result->range->end.line},
                                             {"character", result->range->end.character}}}};
                }

                return hover_json;
            });
    }
};
