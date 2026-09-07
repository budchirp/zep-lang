module;

#include <cstdint>
#include <exception>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

export module zep.lsp.dispatcher;

export class Dispatcher {
  private:
    using RequestHandler = std::function<nlohmann::json(const nlohmann::json&)>;
    using NotificationHandler = std::function<void(const nlohmann::json&)>;
    std::unordered_map<std::string, RequestHandler> request_handlers;
    std::unordered_map<std::string, NotificationHandler> notification_handlers;

    static nlohmann::json error(const nlohmann::json& id, std::int32_t code,
                                const std::string& message) {
        return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    }

  public:
    void on_request(std::string method, RequestHandler handler) {
        if (!request_handlers.emplace(std::move(method), std::move(handler)).second) {
            throw std::invalid_argument("request handler is already registered");
        }
    }

    void on_notification(std::string method, NotificationHandler handler) {
        if (!notification_handlers.emplace(std::move(method), std::move(handler)).second) {
            throw std::invalid_argument("notification handler is already registered");
        }
    }

    std::optional<nlohmann::json> dispatch(const nlohmann::json& message) const {
        if (!message.is_object() || message.value("jsonrpc", std::string()) != "2.0" ||
            !message.contains("method") || !message["method"].is_string()) {
            return error(message.value("id", nlohmann::json()), -32600, "Invalid request");
        }

        auto method = message["method"].get<std::string>();
        auto parameters = message.value("params", nlohmann::json());
        if (!message.contains("id")) {
            if (auto handler = notification_handlers.find(method);
                handler != notification_handlers.end()) {
                try {
                    handler->second(parameters);
                } catch (const std::exception&) {}
            }
            return std::nullopt;
        }

        const auto& id = message["id"];
        auto handler = request_handlers.find(method);
        if (handler == request_handlers.end()) {
            return error(id, -32601, "Method not found: " + method);
        }

        try {
            return nlohmann::json{
                {"jsonrpc", "2.0"}, {"id", id}, {"result", handler->second(parameters)}};
        } catch (const nlohmann::json::exception& exception) {
            return error(id, -32602, exception.what());
        } catch (const std::invalid_argument& exception) {
            return error(id, -32602, exception.what());
        } catch (const std::out_of_range& exception) {
            return error(id, -32602, exception.what());
        } catch (const std::exception& exception) {
            return error(id, -32603, exception.what());
        }
    }
};
